#include "tik/CartographerKernel.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "tik/libtik.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <queue>
#include <spdlog/spdlog.h>

using namespace std;
using namespace llvm;
namespace TraceAtlas::tik
{

    std::set<GlobalVariable *> globalDeclarationSet;
    std::set<Value *> remappedOperandSet;

    void CopyOperand(llvm::User *inst, llvm::ValueToValueMapTy &VMap)
    {
        if (auto func = dyn_cast<Function>(inst))
        {
            auto m = func->getParent();
            if (m != TikModule)
            {
                auto *funcDec = cast<Function>(TikModule->getOrInsertFunction(func->getName(), func->getFunctionType()).getCallee());
                funcDec->setAttributes(func->getAttributes());
                VMap[cast<Value>(func)] = funcDec;
                for (auto arg = funcDec->arg_begin(); arg < funcDec->arg_end(); arg++)
                {
                    auto argVal = cast<Value>(arg);
                    if (auto Use = dyn_cast<User>(argVal))
                    {
                        CopyOperand(Use, VMap);
                    }
                }
            }
        }
        else if (auto *gv = dyn_cast<GlobalVariable>(inst))
        {
            Module *m = gv->getParent();
            if (m != TikModule)
            {
                //its the wrong module
                if (globalDeclarationSet.find(gv) == globalDeclarationSet.end())
                {
                    // iterate through all internal operators of this global
                    if (gv->hasInitializer())
                    {
                        llvm::Constant *value = gv->getInitializer();
                        for (uint32_t iter = 0; iter < value->getNumOperands(); iter++)
                        {
                            auto *internal = cast<llvm::User>(value->getOperand(iter));
                            CopyOperand(internal, VMap);
                        }
                    }
                    //and not already in the vmap

                    //for some reason if we don't do this first the verifier fails
                    //we do absolutely nothing with it and it doesn't even end up in our output
                    //its technically a memory leak, but its an acceptable sacrifice
                    auto *newVar = new GlobalVariable(
                        gv->getValueType(),
                        gv->isConstant(), gv->getLinkage(), nullptr, "",
                        gv->getThreadLocalMode(),
                        gv->getType()->getAddressSpace());
                    newVar->copyAttributesFrom(gv);
                    //end of the sacrifice
                    auto newGlobal = cast<GlobalVariable>(TikModule->getOrInsertGlobal(gv->getName(), gv->getType()->getPointerElementType()));
                    newGlobal->setConstant(gv->isConstant());
                    newGlobal->setLinkage(gv->getLinkage());
                    newGlobal->setThreadLocalMode(gv->getThreadLocalMode());
                    newGlobal->copyAttributesFrom(gv);
                    if (gv->hasInitializer())
                    {
                        newGlobal->setInitializer(MapValue(gv->getInitializer(), VMap));
                    }
                    SmallVector<std::pair<unsigned, MDNode *>, 1> MDs;
                    gv->getAllMetadata(MDs);
                    for (auto MD : MDs)
                    {
                        newGlobal->addMetadata(MD.first, *MapMetadata(MD.second, VMap, RF_MoveDistinctMDs));
                    }
                    if (Comdat *SC = gv->getComdat())
                    {
                        Comdat *DC = newGlobal->getParent()->getOrInsertComdat(SC->getName());
                        DC->setSelectionKind(SC->getSelectionKind());
                        newGlobal->setComdat(DC);
                    }
                    globalDeclarationSet.insert(newGlobal);
                    VMap[gv] = newGlobal;
                    for (auto user : gv->users())
                    {
                        if (auto *newInst = dyn_cast<llvm::Instruction>(user))
                        {
                            if (newInst->getModule() == TikModule)
                            {
                                user->replaceUsesOfWith(gv, newGlobal);
                            }
                        }
                    }
                    // check for arguments within the global variable
                    for (unsigned int i = 0; i < newGlobal->getNumOperands(); i++)
                    {
                        if (auto newOp = dyn_cast<GlobalVariable>(newGlobal->getOperand(i)))
                        {
                            CopyOperand(newOp, VMap);
                        }
                        else if (auto newOp = dyn_cast<Constant>(newGlobal->getOperand(i)))
                        {
                            CopyOperand(newOp, VMap);
                        }
                    }
                }
            }
        }
        for (uint32_t j = 0; j < inst->getNumOperands(); j++)
        {
            if (auto newOp = dyn_cast<GlobalVariable>(inst->getOperand(j)))
            {
                CopyOperand(newOp, VMap);
            }
            else if (auto newFunc = dyn_cast<Function>(inst->getOperand(j)))
            {
                CopyOperand(newFunc, VMap);
            }
        }
    }

    CartographerKernel::CartographerKernel(std::vector<int64_t> basicBlocks, llvm::Module *M, std::string name)
    {
        llvm::ValueToValueMapTy VMap;
        set<int64_t> blockSet;
        for (auto b : basicBlocks)
        {
            blockSet.insert(b);
        }
        string Name;
        if (name.empty())
        {
            Name = "Kernel_" + to_string(KernelUID++);
        }
        else if (name.front() >= '0' && name.front() <= '9')
        {
            Name = "K" + name;
        }
        else
        {
            Name = name;
        }
        if (reservedNames.find(Name) != reservedNames.end())
        {
            throw AtlasException("Kernel Error: Kernel names must be unique!");
        }
        spdlog::debug("Started converting kernel {0}", Name);
        reservedNames.insert(Name);

        set<BasicBlock *> blocks;
        for (auto &F : *M)
        {
            for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
            {
                auto *b = cast<BasicBlock>(BB);
                int64_t id = GetBlockID(b);
                if (id != -1)
                {
                    if (find(basicBlocks.begin(), basicBlocks.end(), id) != basicBlocks.end())
                    {
                        blocks.insert(b);
                    }
                }
            }
        }

        try
        {
            //this is a recursion check, just so we can enumerate issues
            for (auto block : blocks)
            {
                Function *f = block->getParent();
                for (auto bi = block->begin(); bi != block->end(); bi++)
                {
                    if (auto *cb = dyn_cast<CallBase>(bi))
                    {
                        if (cb->getCalledFunction() == f)
                        {
                            throw AtlasException("Tik Error: Recursion is unimplemented")
                        }
                        if (isa<InvokeInst>(cb))
                        {
                            throw AtlasException("Invoke Inst is unsupported")
                        }
                    }
                }
            }

            //SplitBlocks(blocks);
            std::map<llvm::Value *, llvm::GlobalObject *> GlobalMap;

            GetBoundaryValues(blocks);

            //we now have all the information we need
            //start by making the correct function
            std::vector<llvm::Type *> inputArgs;
            inputArgs.push_back(Type::getInt8Ty(TikModule->getContext()));
            for (auto inst : KernelImports)
            {
                inputArgs.push_back(inst->getType());
            }
            for (auto inst : KernelExports)
            {
                inputArgs.push_back(PointerType::get(inst->getType(), 0));
            }
            FunctionType *funcType = FunctionType::get(Type::getInt8Ty(TikModule->getContext()), inputArgs, false);
            KernelFunction = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, Name, TikModule);
            uint64_t i;
            for (i = 0; i < KernelImports.size(); i++)
            {
                auto *a = cast<Argument>(KernelFunction->arg_begin() + 1 + i);
                a->setName("i" + to_string(i));
                VMap[KernelImports[i]] = a;
                ArgumentMap[a] = KernelImports[i];
            }
            uint64_t j;
            for (j = 0; j < KernelExports.size(); j++)
            {
                auto *a = cast<Argument>(KernelFunction->arg_begin() + 1 + i + j);
                a->setName("e" + to_string(j));
                ArgumentMap[a] = KernelExports[j];
            }

            //create the artificial blocks
            Init = BasicBlock::Create(TikModule->getContext(), "Init", KernelFunction);
            Exit = BasicBlock::Create(TikModule->getContext(), "Exit", KernelFunction);
            Exception = BasicBlock::Create(TikModule->getContext(), "Exception", KernelFunction);

            //copy the appropriate blocks
            BuildKernelFromBlocks(VMap, blocks);

            Remap(VMap); //we need to remap before inlining

            InlineFunctionsFromBlocks(blockSet);

            CopyGlobals(VMap);

            //remap and repipe
            Remap(VMap);

            //remap the conditional to the exit
            for (auto ki = KernelFunction->begin(); ki != KernelFunction->end(); ki++)
            {
                auto c = cast<BasicBlock>(ki);
                auto cTerm = c->getTerminator();
                if (cTerm == nullptr)
                {
                    continue;
                }
                uint32_t cSuc = cTerm->getNumSuccessors();
                for (uint32_t i = 0; i < cSuc; i++)
                {
                    auto suc = cTerm->getSuccessor(i);
                    if (suc->getParent() != KernelFunction)
                    {
                        if (ExitBlockMap.find(suc) == ExitBlockMap.end())
                        {
                            BasicBlock *tmpExit = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                            IRBuilder<> exitBuilder(tmpExit);
                            exitBuilder.CreateBr(Exit);
                            ExitBlockMap[suc] = tmpExit;
                        }

                        cTerm->setSuccessor(i, ExitBlockMap[suc]);
                    }
                }
            }

            // replace external function calls with tik declarations
            for (auto &bi : *(KernelFunction))
            {
                for (auto inst = bi.begin(); inst != bi.end(); inst++)
                {
                    if (auto callBase = dyn_cast<CallBase>(inst))
                    {
                        Function *f = callBase->getCalledFunction();
                        if (f == nullptr)
                        {
                            throw AtlasException("Null function call (indirect call)");
                        }

                        auto *funcDec = cast<Function>(TikModule->getOrInsertFunction(callBase->getCalledFunction()->getName(), callBase->getCalledFunction()->getFunctionType()).getCallee());
                        funcDec->setAttributes(callBase->getCalledFunction()->getAttributes());
                        callBase->setCalledFunction(funcDec);
                    }
                }
            }

            BuildInit(VMap);

            BuildExit(VMap);

            RemapNestedKernels(VMap);

            RemapExports(VMap);

            PatchPhis();

            //apply metadata
            ApplyMetadata(GlobalMap);

            //and set a flag that we succeeded
            Valid = true;
        }
        catch (AtlasException &e)
        {
            spdlog::error(e.what());
            if (KernelFunction != nullptr)
            {
                KernelFunction->eraseFromParent();
            }
        }

        try
        {
            //GetKernelLabels();
        }
        catch (AtlasException &e)
        {
            spdlog::warn("Failed to annotate Loop/Memory grammars");
            spdlog::debug(e.what());
        }
    }

    CartographerKernel::CartographerKernel(Function* kernFunc)
    {
        KernelFunction = kernFunc;
        Name = KernelFunction->getName();
        // get the metadata and parse it into entrance, exit, values
        //     turn metadata into string
        //     parse json and populate Entrances, Exits,
        MDNode *meta = KernelFunction->getMetadata("Boundaries");
        std::string metaString;
        if( auto mstring = dyn_cast<MDString>(meta) )
        {
            metaString = mstring->getString();
        }
        else
        {
            AtlasException("Could not convert metadata into string.");
        }
        std::cout << "metadata string is " << metaString << std::endl;
    }

    void CartographerKernel::GetBoundaryValues(set<BasicBlock *> &blocks)
    {
        int exitId = 0;
        set<BasicBlock *> EntranceBlocks;
        set<BasicBlock *> coveredExits;
        for (auto block : blocks)
        {
            //we start with entrances
            //formerly GetEntrances
            //to check for entrances we look at the parent function first
            Function *F = block->getParent();
            BasicBlock *par = &F->getEntryBlock();
            //we first check if the block is an entry block, if it is the only way it could be an entry is through a function call
            if (block == par)
            {
                bool exte = false;
                bool inte = false;
                for (auto user : F->users())
                {
                    if (auto cb = dyn_cast<CallBase>(user))
                    {
                        BasicBlock *parent = cb->getParent(); //the parent of the function call
                        if (blocks.find(parent) == blocks.end())
                        {
                            exte = true;
                        }
                        else
                        {
                            inte = true;
                        }
                    }
                }
                if (exte && !inte)
                {
                    //exclusively external so this is an entrance
                    EntranceBlocks.insert(block);
                }
                else if (exte && inte)
                {
                    //both external and internal, so maybe an entrance
                    //ignore for the moment, worst case we will have no entrances
                    //throw AtlasException("Mixed function uses, not implemented");
                }
                else if (!exte && inte)
                {
                    //only internal, so ignore
                }
                else
                {
                    //neither internal or external, throw error
                    throw AtlasException("Function with no internal or external uses");
                }
            }
            else
            {
                //this isn't an entry block, therefore we apply the new algorithm
                bool ent = false;
                queue<BasicBlock *> workingSet;
                set<BasicBlock *> visitedBlocks;
                workingSet.push(block);
                visitedBlocks.insert(block);
                while (!workingSet.empty())
                {
                    BasicBlock *current = workingSet.back();
                    workingSet.pop();
                    if (current == par)
                    {
                        //this is the entry block. We know that there is a valid path through the computation to here
                        //that doesn't somehow originate in the kernel
                        //this is guaranteed by the prior type 2 detector in cartographer
                        ent = true;
                        break;
                    }
                    for (BasicBlock *pred : predecessors(current))
                    {
                        //we now add every predecessor to the working set as long as
                        //1. we haven't visited it before
                        //2. it is not inside the kernel
                        //we are trying to find the entrance to the function because it is was indicates a true entrance
                        if (visitedBlocks.find(pred) == visitedBlocks.end() && blocks.find(pred) == blocks.end())
                        {
                            visitedBlocks.insert(pred);
                            workingSet.push(pred);
                        }
                    }
                }
                if (ent)
                {
                    //this is assumed to be a true entrance
                    //if false it has no path that doesn't pass through the prior kernel
                    EntranceBlocks.insert(block);
                }
            }

            //we now look for exits through the successors and return instructions
            //formerly GetExits
            for (auto suc : successors(block))
            {
                if (blocks.find(suc) == blocks.end())
                {
                    if (coveredExits.find(suc) == coveredExits.end())
                    {
                        ExitTarget[exitId++] = suc;
                        coveredExits.insert(suc);
                    }
                }
            }
            auto term = block->getTerminator();
            if (auto retInst = dyn_cast<ReturnInst>(term))
            {
                Function *base = block->getParent();
                for (auto user : base->users())
                {
                    auto *v = cast<Instruction>(user);
                    if (blocks.find(v->getParent()) == blocks.end())
                    {
                        if (coveredExits.find(v->getParent()) == coveredExits.end())
                        {
                            ExitTarget[exitId++] = v->getParent();
                            coveredExits.insert(v->getParent());
                        }
                    }
                }
            }

            //we now finally ask for the external values
            //formerly GetExternalValues
            for (BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; ++BI)
            {
                auto *inst = cast<Instruction>(BI);
                //start by getting all the inputs
                //they will be composed of the operands whose input is not defined in one of the parent blocks
                uint32_t numOps = inst->getNumOperands();
                for (uint32_t i = 0; i < numOps; i++)
                {
                    Value *op = inst->getOperand(i);
                    if (auto *operand = dyn_cast<Instruction>(op))
                    {
                        BasicBlock *parentBlock = operand->getParent();
                        if (std::find(blocks.begin(), blocks.end(), parentBlock) == blocks.end())
                        {
                            if (find(KernelImports.begin(), KernelImports.end(), operand) == KernelImports.end())
                            {
                                KernelImports.push_back(operand);
                            }
                        }
                    }
                    else if (auto *ar = dyn_cast<Argument>(op))
                    {
                        if (auto *ci = dyn_cast<CallInst>(inst))
                        {
                            if (KfMap.find(ci->getCalledFunction()) != KfMap.end())
                            {
                                auto subKernel = KfMap[ci->getCalledFunction()];
                                for (auto sExtVal : subKernel->KernelImports)
                                {
                                    //these are the arguments for the function call in order
                                    //we now can check if they are in our vmap, if so they aren't external
                                    //if not they are and should be mapped as is appropriate
                                    if (find(KernelImports.begin(), KernelImports.end(), sExtVal) == KernelImports.end())
                                    {
                                        KernelImports.push_back(sExtVal);
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (find(KernelImports.begin(), KernelImports.end(), ar) == KernelImports.end())
                            {
                                KernelImports.push_back(ar);
                            }
                        }
                    }
                }

                //then get all the exports
                //this is composed of all the instructions whose use extends beyond the current blocks
                for (auto user : inst->users())
                {
                    if (auto i = dyn_cast<Instruction>(user))
                    {
                        auto p = i->getParent();
                        if (blocks.find(p) == blocks.end())
                        {
                            //the use is external therefore it should be a kernel export
                            KernelExports.push_back(inst);
                            break;
                        }
                    }
                    else
                    {
                        throw AtlasException("Non-instruction user detected");
                    }
                }
            }
        }
        if (EntranceBlocks.empty())
        {
            throw AtlasException("Kernel Exception: tik requires a body entrance");
        }
        if (exitId == 0)
        {
            throw AtlasException("Tik Error: tik found no kernel exits")
        }
        // populate Entrance vector and map
        for (auto entry : EntranceBlocks)
        {
            int64_t blockID = GetBlockID(entry);
            Entrances.insert(blockID);
            EntranceMap[blockID] = entry;
        }
    }

    void CartographerKernel::BuildKernelFromBlocks(llvm::ValueToValueMapTy &VMap, set<BasicBlock *> &blocks)
    {
        set<Function *> headFunctions;
        for (auto ent : Entrances)
        {
            headFunctions.insert(EntranceMap[ent]->getParent());
        }

        if (headFunctions.size() != 1)
        {
            throw AtlasException("Entrances not on same level");
        }

        set<BasicBlock *> handledExits;
        for (auto block : blocks)
        {
            if (headFunctions.find(block->getParent()) == headFunctions.end())
            {
                continue;
            }
            int64_t id = GetBlockID(block);
            if (KernelMap.find(id) != KernelMap.end())
            {
                //this belongs to a subkernel
                auto nestedKernel = KernelMap[id];
                if (nestedKernel->Entrances.find(GetBlockID(block)) != nestedKernel->Entrances.end())
                {
                    //we need to make a unique block for each entrance (there is currently only one)
                    //int i = 0;
                    //for (auto ent : nestedKernel->Entrances)
                    for (uint64_t i = 0; i < nestedKernel->Entrances.size(); i++)
                    {
                        std::vector<llvm::Value *> inargs;
                        for (auto ai = nestedKernel->KernelFunction->arg_begin(); ai < nestedKernel->KernelFunction->arg_end(); ai++)
                        {
                            if (ai == nestedKernel->KernelFunction->arg_begin())
                            {
                                inargs.push_back(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), i));
                            }
                            else
                            {
                                inargs.push_back(cast<Value>(ai));
                            }
                        }
                        BasicBlock *intermediateBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                        IRBuilder<> intBuilder(intermediateBlock);
                        auto cc = intBuilder.CreateCall(nestedKernel->KernelFunction, inargs);
                        MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt1Ty(TikModule->getContext()), 1)));
                        SetBlockID(intermediateBlock, -2);
                        cc->setMetadata("KernelCall", tikNode);
                        auto sw = intBuilder.CreateSwitch(cc, Exception, (uint32_t)nestedKernel->ExitTarget.size());
                        for (auto pair : nestedKernel->ExitTarget)
                        {

                            if (blocks.find(pair.second) != blocks.end())
                            {
                                sw->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t)pair.first), pair.second);
                            }
                            else
                            {
                                if (handledExits.find(pair.second) == handledExits.end())
                                {
                                    //exits both kernels simultaneously
                                    //we create a temp block and remab the exit so the phi has a value
                                    //then remap in the dictionary for the final mapping
                                    //note that we do not change the ExitTarget map so we still go to the right place

                                    BasicBlock *tar = nullptr;
                                    //we need to find every block in the nested kernel that will branch to this target
                                    //easiest way to do this is to go through every block in this kernel and check if it is in the nested kernel
                                    set<BasicBlock *> nExits;
                                    for (auto k : nestedKernel->ExitMap)
                                    {
                                        if (k.second == pair.first)
                                        {
                                            //this is the exit
                                            nExits.insert(k.first);
                                        }
                                    }

                                    if (nExits.size() != 1)
                                    {
                                        throw AtlasException("Expected exactly one exit from nested kernel");
                                    }
                                    tar = *nExits.begin();
                                    BasicBlock *newBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                                    IRBuilder<> newBlockBuilder(newBlock);
                                    newBlockBuilder.CreateBr(Exit);
                                    sw->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t)pair.first), newBlock);
                                    int index = ExitMap[tar];
                                    ExitMap.erase(tar);
                                    ExitMap[newBlock] = index;
                                    handledExits.insert(pair.second);
                                }
                            }
                        }
                        VMap[block] = intermediateBlock;
                        //i++;
                    }
                }
                else
                {
                    //this is a block from the nested kernel
                    //it doesn't need to be mapped
                }
            }
            else
            {
                auto cb = CloneBasicBlock(block, VMap, "", KernelFunction);
                VMap[block] = cb;
                // add medadata to this block to remember what its original predecessor was, for swapping later
                //MDNode* oldID = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(block->getParent()->getParent()->getContext()), block->getValueID())));
                //cast<Instruction>(cb->getFirstInsertionPt())->setMetadata("oldName", oldID);
                if (Conditional.find(block) != Conditional.end())
                {
                    Conditional.erase(block);
                    Conditional.insert(cb);
                }

                //fix the phis
                int rescheduled = 0; //the number of blocks we rescheduled
                for (auto bi = cb->begin(); bi != cb->end(); bi++)
                {
                    if (auto *p = dyn_cast<PHINode>(bi))
                    {
                        for (auto pred : p->blocks())
                        {
                            if (blocks.find(pred) == blocks.end())
                            {
                                //we have an invalid predecessor, replace with init
                                if (Entrances.find(GetBlockID(block)) != Entrances.end())
                                {
                                    p->replaceIncomingBlockWith(pred, Init);
                                    rescheduled++;
                                }
                                else
                                {
                                    auto a = p->getBasicBlockIndex(pred);
                                    if (a != -1)
                                    {
                                        p->removeIncomingValue(pred);
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                if (rescheduled > 1)
                {
                    spdlog::warn("Rescheduled more than one phi predecessor"); //basically this is a band aid. Needs some more help
                }
            }
        }
    }

    void CartographerKernel::InlineFunctionsFromBlocks(std::set<int64_t> &blocks)
    {
        bool change = true;
        while (change)
        {
            change = false;
            for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
            {
                auto baseBlock = cast<BasicBlock>(fi);
                auto id = GetBlockID(baseBlock);
                if (blocks.find(id) == blocks.end())
                {
                    continue;
                }
                for (auto bi = fi->begin(); bi != fi->end(); bi++)
                {
                    if (auto ci = dyn_cast<CallInst>(bi))
                    {
                        if (auto debug = ci->getMetadata("KernelCall"))
                        {
                            continue;
                        }
                        auto id = GetBlockID(baseBlock);
                        auto info = InlineFunctionInfo();
                        auto r = InlineFunction(ci, info);
                        SetBlockID(baseBlock, id);
                        if (r)
                        {
                            change = true;
                        }
                        break;
                    }
                }
            }
        }
        //erase null blocks here
        auto blockList = &KernelFunction->getBasicBlockList();
        vector<Function::iterator> toRemove;

        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            if (auto b = dyn_cast<BasicBlock>(fi))
            {
                //do nothing
            }
            else
            {
                toRemove.push_back(fi);
            }
        }

        for (auto r : toRemove)
        {
            blockList->erase(r);
        }

        //now that everything is inlined we need to remove invalid blocks
        //although some blocks are now an amalgamation of multiple,
        //as a rule we don't need to worry about those.
        //simple successors are enough
        vector<BasicBlock *> bToRemove;
        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto block = cast<BasicBlock>(fi);
            int64_t id = GetBlockID(block);
            if (blocks.find(id) == blocks.end() && block != Exit && block != Init && block != Exception && id != -2)
            {
                for (auto user : block->users())
                {
                    if (auto *phi = dyn_cast<PHINode>(user))
                    {
                        phi->removeIncomingValue(block);
                    }
                    else
                    {
                        user->replaceUsesOfWith(block, Exit);
                    }
                }
                bToRemove.push_back(block);
            }
        }
        /*
    for (auto block : bToRemove)
    {
        //this breaks hard for some reason
        //not really necessary fortunately
        //block->eraseFromParent();
    }
    */
    }

    void CartographerKernel::RemapNestedKernels(llvm::ValueToValueMapTy &VMap)
    {
        // Now find all calls to the embedded kernel functions in the body, if any, and change their arguments to the new ones
        std::map<Argument *, Value *> embeddedCallArgs;
        for (auto &bf : *(KernelFunction))
        {
            for (BasicBlock::iterator i = bf.begin(), BE = bf.end(); i != BE; ++i)
            {
                if (auto *callInst = dyn_cast<CallInst>(i))
                {
                    auto calledFunc = callInst->getCalledFunction();
                    auto subK = KfMap[calledFunc];
                    if (subK != nullptr)
                    {
                        for (auto sarg = calledFunc->arg_begin(); sarg < calledFunc->arg_end(); sarg++)
                        {
                            for (auto &b : *(KernelFunction))
                            {
                                for (BasicBlock::iterator j = b.begin(), BE2 = b.end(); j != BE2; ++j)
                                {
                                    auto inst = cast<Instruction>(j);
                                    auto subArg = subK->ArgumentMap[sarg];
                                    if (subArg != nullptr)
                                    {
                                        if (subK->ArgumentMap[sarg] == inst)
                                        {
                                            embeddedCallArgs[sarg] = inst;
                                        }
                                        else if (VMap[subK->ArgumentMap[sarg]] == inst)
                                        {
                                            embeddedCallArgs[sarg] = inst;
                                        }
                                    }
                                }
                            }
                        }
                        for (auto sarg = calledFunc->arg_begin(); sarg < calledFunc->arg_end(); sarg++)
                        {
                            for (auto arg = KernelFunction->arg_begin(); arg < KernelFunction->arg_end(); arg++)
                            {
                                if (subK->ArgumentMap[sarg] == ArgumentMap[arg])
                                {
                                    embeddedCallArgs[sarg] = arg;
                                }
                                else if (VMap[subK->ArgumentMap[sarg]] == ArgumentMap[arg])
                                {
                                    embeddedCallArgs[sarg] = arg;
                                }
                            }
                        }
                        auto limit = callInst->getNumArgOperands();
                        for (uint32_t k = 1; k < limit; k++)
                        {
                            Value *op = callInst->getArgOperand(k);
                            if (auto *arg = dyn_cast<Argument>(op))
                            {
                                if (embeddedCallArgs.find(arg) == embeddedCallArgs.end())
                                {
                                    throw AtlasException("Failed to find nested argument");
                                }
                                auto asdf = embeddedCallArgs[arg];
                                callInst->setArgOperand(k, asdf);
                            }
                            else
                            {
                                throw AtlasException("Tik Error: Unexpected value passed to function");
                            }
                        }
                    }
                }
            }
        }
    }

    void CartographerKernel::RemapExports(llvm::ValueToValueMapTy &VMap)
    {
        map<Value *, AllocaInst *> exportMap;
        for (auto ex : KernelExports)
        {
            Value *mapped = VMap[ex];
            if (mapped != nullptr)
            {
                if (mapped->getNumUses() != 0)
                {
                    IRBuilder iBuilder(Init->getFirstNonPHI());
                    AllocaInst *alloc = iBuilder.CreateAlloca(mapped->getType());
                    exportMap[mapped] = alloc;
                    for (auto u : mapped->users())
                    {
                        if (auto *p = dyn_cast<PHINode>(u))
                        {
                            for (uint32_t i = 0; i < p->getNumIncomingValues(); i++)
                            {
                                if (mapped == p->getIncomingValue(i))
                                {
                                    BasicBlock *prev = p->getIncomingBlock(i);
                                    IRBuilder<> phiBuilder(prev->getTerminator());
                                    auto load = phiBuilder.CreateLoad(alloc);
                                    p->setIncomingValue(i, load);
                                }
                            }
                        }
                        else
                        {
                            IRBuilder<> uBuilder(cast<Instruction>(u));
                            auto load = uBuilder.CreateLoad(alloc);
                            for (uint32_t i = 0; i < u->getNumOperands(); i++)
                            {
                                if (mapped == u->getOperand(i))
                                {
                                    u->setOperand(i, load);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto block = cast<BasicBlock>(fi);
            for (auto bi = fi->begin(); bi != fi->end(); bi++)
            {
                auto i = cast<Instruction>(bi);
                if (auto call = dyn_cast<CallInst>(i))
                {
                    if (call->getMetadata("KernelCall") != nullptr)
                    {
                        Function *F = call->getCalledFunction();
                        auto fType = F->getFunctionType();
                        for (uint32_t i = 0; i < call->getNumArgOperands(); i++)
                        {
                            auto arg = call->getArgOperand(i);
                            if (arg == nullptr)
                            {
                                continue;
                            }
                            if (arg->getType() != fType->getParamType(i))
                            {
                                IRBuilder<> aBuilder(call);
                                auto load = aBuilder.CreateLoad(arg);
                                call->setArgOperand(i, load);
                            }
                        }
                    }
                }
                if (exportMap.find(i) != exportMap.end())
                {
                    Instruction *buildBase;
                    if (isa<PHINode>(i))
                    {
                        buildBase = block->getFirstNonPHI();
                    }
                    else
                    {
                        buildBase = i->getNextNode();
                    }
                    IRBuilder<> b(buildBase);
                    b.CreateStore(i, exportMap[i]);
                    bi++;
                }
            }
        }
    }

    void CartographerKernel::CopyGlobals(llvm::ValueToValueMapTy &VMap)
    {
        for (auto &fi : *(KernelFunction))
        {
            for (auto bi = fi.begin(); bi != fi.end(); bi++)
            {
                auto inst = cast<Instruction>(bi);
                if (auto cv = dyn_cast<CallBase>(inst))
                {
                    for (auto i = cv->arg_begin(); i < cv->arg_end(); i++)
                    {
                        if (auto user = dyn_cast<User>(i))
                        {
                            CopyOperand(user, VMap);
                        }
                    }
                }
                else
                {
                    CopyOperand(inst, VMap);
                }
            }
        }
    }

    void CartographerKernel::BuildInit(llvm::ValueToValueMapTy &VMap)
    {
        IRBuilder<> initBuilder(Init);
        auto initSwitch = initBuilder.CreateSwitch(KernelFunction->arg_begin(), Exception, (uint32_t)Entrances.size());
        uint64_t i = 0;
        for (auto ent : Entrances)
        {
            if (KernelMap.find(ent) == KernelMap.end() && VMap[EntranceMap[ent]] != nullptr)
            {
                initSwitch->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), i), cast<BasicBlock>(VMap[EntranceMap[ent]]));
            }
            else
            {
                throw AtlasException("Unimplemented");
            }
            i++;
        }
    }

    void CartographerKernel::BuildExit(llvm::ValueToValueMapTy &VMap)
    {
        PrintVal(Exit, false); //another sacrifice
        IRBuilder<> exitBuilder(Exit);
        int i = 0;
        for (auto pred : predecessors(Exit))
        {
            ExitMap[pred] = i;
            i++;
        }

        auto phi = exitBuilder.CreatePHI(Type::getInt8Ty(TikModule->getContext()), (uint32_t)ExitMap.size());
        for (auto pair : ExitMap)
        {
            Value *v;
            if (pair.first->getModule() == TikModule)
            {
                v = pair.first;
            }
            else
            {
                v = VMap[pair.first];
            }
            phi->addIncoming(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t)pair.second), cast<BasicBlock>(v));
        }

        exitBuilder.CreateRet(phi);

        IRBuilder<> exceptionBuilder(Exception);
        exceptionBuilder.CreateRet(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t)-2));
    }

    void CartographerKernel::PatchPhis()
    {
        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto b = cast<BasicBlock>(fi);
            vector<PHINode *> phisToRemove;
            for (auto &phi : b->phis())
            {
                vector<BasicBlock *> valuesToRemove;
                for (uint32_t i = 0; i < phi.getNumIncomingValues(); i++)
                {
                    auto block = phi.getIncomingBlock(i);
                    if (block->getParent() != KernelFunction)
                    {
                        valuesToRemove.push_back(block);
                    }
                    else
                    {
                        bool isPred = false;
                        for (auto pred : predecessors(b))
                        {
                            if (pred == block)
                            {
                                isPred = true;
                            }
                        }
                        if (!isPred)
                        {
                            valuesToRemove.push_back(block);
                        }
                    }
                }
                for (auto toR : valuesToRemove)
                {
                    phi.removeIncomingValue(toR, false);
                }
                if (phi.getNumIncomingValues() == 0)
                {
                    phisToRemove.push_back(&phi);
                }
            }
            for (auto phi : phisToRemove)
            {
                phi->eraseFromParent();
            }
        }
    }

    void RemapOperands(User *op, Instruction *inst, llvm::ValueToValueMapTy &VMap)
    {
        if (remappedOperandSet.find(op) == remappedOperandSet.end())
        {
            IRBuilder Builder(inst);
            // if its a gep or load we need to make a new one (because gep and load args can't be changed after construction)
            if (auto gepInst = dyn_cast<GEPOperator>(op))
            {
                // duplicate the indexes of the GEP
                vector<Value *> idxList;
                for (auto idx = gepInst->idx_begin(); idx != gepInst->idx_end(); idx++)
                {
                    auto indexValue = cast<Value>(idx);
                    idxList.push_back(indexValue);
                }
                // find out what our pointer needs to be
                Value *ptr;
                if (auto gepPtr = dyn_cast<GlobalVariable>(gepInst->getPointerOperand()))
                {
                    if (gepPtr->getParent() != TikModule)
                    {
                        if (globalDeclarationSet.find(gepPtr) == globalDeclarationSet.end())
                        {
                            CopyOperand(gepInst, VMap);
                            ptr = VMap[gepPtr];
                            // sanity check
                            if (ptr == nullptr)
                            {
                                throw AtlasException("Declared global not mapped");
                            }
                        }
                        else
                        {
                            ptr = gepPtr;
                        }
                    }
                    else
                    {
                        ptr = gepPtr;
                    }
                    // finally, construct the new GEP and remap its old value
                    Value *newGep = Builder.CreateGEP(ptr, idxList, gepInst->getName());
                    VMap[cast<Value>(op)] = cast<Value>(newGep);
                }
                // we don't see a global here so don't replace the GEPOperator
                else
                {
                }
            }
            else if (auto loadInst = dyn_cast<LoadInst>(op))
            {
                // find out what our pointer needs to be
                Value *ptr;
                if (auto loadPtr = dyn_cast<GlobalVariable>(loadInst->getPointerOperand()))
                {
                    if (loadPtr->getParent() != TikModule)
                    {
                        if (globalDeclarationSet.find(loadPtr) == globalDeclarationSet.end())
                        {
                            CopyOperand(loadInst, VMap);
                            ptr = VMap[loadPtr];
                            // sanity check
                            if (ptr == nullptr)
                            {
                                throw AtlasException("Declared global not mapped");
                            }
                        }
                        else
                        {
                            ptr = loadPtr;
                        }
                    }
                    else
                    {
                        ptr = loadPtr;
                    }
                    Value *newLoad = Builder.CreateLoad(ptr, loadInst->getName());
                    VMap[cast<Value>(op)] = cast<Value>(newLoad);
                }
                // we don't see a global here so don't replace the loadInst
                else
                {
                }
            }
        }
        for (unsigned int operand = 0; operand < op->getNumOperands(); operand++)
        {
            Instruction *newInst = inst;
            if (auto test = dyn_cast<Instruction>(op))
            {
                newInst = test;
            }
            auto opi = op->getOperand(operand);
            if (opi != nullptr)
            {
                if (auto newGlob = dyn_cast<GlobalVariable>(opi))
                {
                    CopyOperand(newGlob, VMap);
                }
                else if (auto newOp = dyn_cast<Operator>(opi))
                {
                    if (remappedOperandSet.find(newOp) == remappedOperandSet.end())
                    {
                        remappedOperandSet.insert(newOp);
                        RemapOperands(newOp, newInst, VMap);
                    }
                }
            }
        }
    }

    void CartographerKernel::Remap(ValueToValueMapTy &VMap)
    {
        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto BB = cast<BasicBlock>(fi);
            for (BasicBlock::iterator bi = BB->begin(); bi != BB->end(); bi++)
            {
                auto *inst = cast<Instruction>(bi);
                for (unsigned int arg = 0; arg < inst->getNumOperands(); arg++)
                {
                    Value *inputOp = inst->getOperand(arg);
                    if (auto op = dyn_cast<Operator>(inputOp))
                    {
                        RemapOperands(op, inst, VMap);
                    }
                }
                RemapInstruction(inst, VMap, llvm::RF_None);
            }
        }
    }
} // namespace TraceAtlas::tik
