#include "tik/CartographerKernel.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "tik/Util.h"
#include "tik/libtik.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <nlohmann/json.hpp>
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
            if (auto newGP = dyn_cast<GlobalVariable>(inst->getOperand(j)))
            {
                CopyOperand(newGP, VMap);
            }
            else if (auto newFunc = dyn_cast<Function>(inst->getOperand(j)))
            {
                CopyOperand(newFunc, VMap);
            }
            else if (auto newOp = dyn_cast<GEPOperator>(inst->getOperand(j)))
            {
                CopyOperand(newOp, VMap);
            }
            else if (auto newBitCast = dyn_cast<BitCastOperator>(inst->getOperand(j)))
            {
                CopyOperand(newBitCast, VMap);
            }
        }
    }

    CartographerKernel::CartographerKernel(std::vector<int64_t> basicBlocks, std::string name)
    {
        llvm::ValueToValueMapTy VMap;
        auto blockSet = set<int64_t>(basicBlocks.begin(), basicBlocks.end());
        set<BasicBlock *> blocks;
        for (auto id : blockSet)
        {
            if (IDToBlock.find(id) == IDToBlock.end())
            {
                throw AtlasException("Found a basic block with no ID!");
            }
            blocks.insert(IDToBlock[id]);
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
                        // if this is the parent function, its on our context level so don't
                        if (cb->getCalledFunction() == f)
                        {
                            throw AtlasException("Tik Error: Recursion is unimplemented")
                        }
                    }
                }
            }

            //SplitBlocks(blocks);
            map<Value *, GlobalObject *> GlobalMap;
            vector<int64_t> KernelImports;
            vector<int64_t> KernelExports;
            GetBoundaryValues(blocks, KernelImports, KernelExports, VMap);
            //we now have all the information we need
            //start by making the correct function
            vector<Type *> inputArgs;
            // First arg is always the Entrance index
            inputArgs.push_back(Type::getInt8Ty(TikModule->getContext()));
            for (auto inst : KernelImports)
            {
                if (IDToValue.find(inst) != IDToValue.end())
                {
                    inputArgs.push_back(IDToValue[inst]->getType());
                }
                else if (IDToBlock.find(inst) != IDToBlock.end())
                {
                    inputArgs.push_back(IDToBlock[inst]->getType());
                }
                else
                {
                    throw AtlasException("Tried to push a nullptr into the inputArgs when parsing imports.");
                }
            }
            for (auto inst : KernelExports)
            {
                if (IDToValue.find(inst) != IDToValue.end())
                {
                    inputArgs.push_back(IDToValue[inst]->getType());
                }
                else if (IDToBlock.find(inst) != IDToBlock.end())
                {
                    inputArgs.push_back(IDToBlock[inst]->getType());
                }
                else
                {
                    throw AtlasException("Tried to push a nullptr into the inputArgs when parsing imports.");
                }
            }
            FunctionType *funcType = FunctionType::get(Type::getInt8Ty(TikModule->getContext()), inputArgs, false);
            KernelFunction = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, Name, TikModule);
            uint64_t i;
            for (i = 0; i < KernelImports.size(); i++)
            {
                auto *a = cast<Argument>(KernelFunction->arg_begin() + 1 + i);
                a->setName("i" + to_string(i));
                VMap[IDToValue[KernelImports[i]]] = a;
                ArgumentMap[a] = KernelImports[i];
            }
            uint64_t j;
            for (j = 0; j < KernelExports.size(); j++)
            {
                auto *a = cast<Argument>(KernelFunction->arg_begin() + 1 + i + j);
                a->setName("e" + to_string(j));
                VMap[IDToValue[KernelExports[j]]] = a;
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

            BuildExit();

            RemapNestedKernels(VMap);

            PatchPhis(VMap);

            FixInvokes();

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
    }

    void CartographerKernel::GetBoundaryValues(set<BasicBlock *> &blocks, vector<int64_t> &KernelImports, vector<int64_t> &KernelExports, ValueToValueMapTy &VMap)
    {
        // first, add all blocks of function calls. they will later be inlined and therefore do not need to be included
        // we also need the functions so that any args from embedded function calls are skipped as well
        set<BasicBlock *> scopedBlocks = blocks;
        set<Function *> scopedFuncs;
        for (auto block : blocks)
        {
            // find all functions that will be inlined and add them to the scope of the basic block set
            for (auto BI = block->begin(), BE = block->end(); BI != BE; ++BI)
            {
                auto inst = cast<Instruction>(BI);
                // if this is an embedded function call
                if (auto *ci = dyn_cast<CallInst>(inst))
                {
                    if (ci->getCalledFunction() == nullptr)
                    {
                        throw AtlasException("Null function call (indirect call)");
                    }
                    // if it is not a kernel call
                    if (KfMap.find(ci->getCalledFunction()) == KfMap.end())
                    {
                        // if the function will be inlined
                        //if( !ci->getCalledFunction()->hasFnAttribute("noinline") )
                        //{
                        scopedFuncs.insert(ci->getCalledFunction());
                        // add each of the function blocks
                        for (auto fi = ci->getCalledFunction()->begin(); fi != ci->getCalledFunction()->end(); fi++)
                        {
                            auto BB = cast<BasicBlock>(fi);
                            scopedBlocks.insert(BB);
                        }
                        //}
                    }
                }
                for (unsigned int i = 0; i < inst->getNumOperands(); i++)
                {
                    auto op = inst->getOperand(i);
                    // if this is an embedded function call
                    if (auto *ci = dyn_cast<CallInst>(op))
                    {
                        if (ci->getCalledFunction() == nullptr)
                        {
                            throw AtlasException("Null function call (indirect call)");
                        }
                        // if it is not a kernel call
                        if (KfMap.find(ci->getCalledFunction()) == KfMap.end())
                        {
                            // if the function will be inlined
                            //if( !ci->getCalledFunction()->hasFnAttribute("noinline") )
                            //{
                            scopedFuncs.insert(ci->getCalledFunction());
                            // add each of the function blocks
                            for (auto fi = ci->getCalledFunction()->begin(); fi != ci->getCalledFunction()->end(); fi++)
                            {
                                auto BB = cast<BasicBlock>(fi);
                                scopedBlocks.insert(BB);
                            }
                            //}
                        }
                    }
                }
            }
        }
        //we start with entrances
        auto ent = GetEntrances(blocks);
        int entranceId = 0;
        for (auto e : ent)
        {
            IDToBlock[GetBlockID(e)] = e;
            Entrances.insert(make_shared<KernelInterface>(entranceId++, GetBlockID(e)));
        }
        for (auto block : scopedBlocks)
        {
            for (BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; ++BI)
            {
                auto *inst = cast<Instruction>(BI);
                BasicBlock *parentBlock = inst->getParent();
                if (std::find(scopedBlocks.begin(), scopedBlocks.end(), parentBlock) != scopedBlocks.end())
                {
                    for (auto &use : inst->uses())
                    {

                        if (auto useInst = dyn_cast<Instruction>(use.getUser()))
                        {
                            if (find(scopedBlocks.begin(), scopedBlocks.end(), useInst->getParent()) == scopedBlocks.end())
                            {
                                // we need to have an export for this value
                                KernelExports.push_back(GetValueID(inst));
                            }
                        }
                        else if (auto useArg = dyn_cast<Argument>(use.getUser()))
                        {
                            if (find(scopedFuncs.begin(), scopedFuncs.end(), useArg->getParent()) == scopedFuncs.end())
                            {
                                // we need to have an export for this value
                                KernelExports.push_back(GetValueID(useArg));
                            }
                        }
                    }
                }
                if (auto *ci = dyn_cast<CallInst>(inst))
                {
                    if (KfMap.find(ci->getCalledFunction()) != KfMap.end())
                    {
                        auto subKernel = KfMap[ci->getCalledFunction()];
                        for (auto arg = subKernel->KernelFunction->arg_begin(); arg < subKernel->KernelFunction->arg_end(); arg++)
                        {
                            auto sExtVal = GetValueID(cast<Value>(arg));
                            //these are the arguments for the function call in order
                            //we now can check if they are in our vmap, if so they aren't external
                            //if not they are and should be mapped as is appropriate
                            if (VMap.find(arg) == VMap.end())
                            {
                                if (find(KernelImports.begin(), KernelImports.end(), sExtVal) == KernelImports.end())
                                {
                                    KernelImports.push_back(sExtVal);
                                }
                            }
                        }
                    }
                    /*for( unsigned int i = 0; i < ci->getNumArgOperands(); i++)
                    {
                        auto val = ci->getArgOperand(i);
                        if( auto argval = dyn_cast<Argument>(val) )
                        {
                            if (scopedFuncs.find(argval->getParent()) == scopedFuncs.end())
                            {
                                // we found an argument of the callinst that came from somewhere else
                                if (find(KernelImports.begin(), KernelImports.end(), GetValueID(argval)) == KernelImports.end())
                                {
                                    //PrintVal(arg->getParent());
                                    KernelImports.push_back(GetValueID(argval));
                                }
                            }
                        }
                        else if (auto *operand = dyn_cast<Instruction>(val))
                        {
                            BasicBlock *parentBlock = operand->getParent();
                            if (std::find(scopedBlocks.begin(), scopedBlocks.end(), parentBlock) == scopedBlocks.end())
                            {
                                if (find(KernelImports.begin(), KernelImports.end(), GetValueID(operand)) == KernelImports.end())
                                {
                                    KernelImports.push_back(GetValueID(operand));
                                }
                            }
                            // see if this value needs to be exported
                            else
                            {
                                for ( auto& use : operand->uses() )
                                {
                                    if( auto useInst = dyn_cast<Instruction>(use))
                                    {
                                        if( find(scopedBlocks.begin(), scopedBlocks.end(), useInst->getParent()) == scopedBlocks.end())
                                        {
                                            // we need to have an export for this value
                                            PrintVal(useInst);
                                        }
                                    }
                                    else if( auto useArg = dyn_cast<Argument>(use))
                                    {
                                        if( find(scopedFuncs.begin(), scopedFuncs.end(), useArg->getParent()) == scopedFuncs.end())
                                        {
                                            // we need to have an export for this value
                                            PrintVal(useInst);
                                        }                                
                                    }
                                }
                            }
                        }
                    }*/
                }
                for (uint32_t i = 0; i < inst->getNumOperands(); i++)
                {
                    Value *op = inst->getOperand(i);
                    int64_t valID = GetValueID(op);
                    if (valID < IDState::Artificial)
                    {
                        if (auto testBlock = dyn_cast<BasicBlock>(op))
                        {
                            if (GetBlockID(testBlock) < IDState::Artificial)
                            {
                                throw AtlasException("Found a basic block in the bitcode that did not have a blockID.");
                            }
                        }
                        // check to see if this object can have metadata
                        else if (auto testInst = dyn_cast<Instruction>(op))
                        {
                            throw AtlasException("Found a value in the bitcode that did not have a valueID.");
                        }
                        else if (auto testGO = dyn_cast<GlobalObject>(op))
                        {
                            throw AtlasException("Found a global object in the bitcode that did not have a valueID.");
                        }
                        else if (auto arg = dyn_cast<Argument>(op))
                        {
                            throw AtlasException("Found an argument in the bitcode that did not have a valueID.");
                        }
                        else
                        {
                            // its not an instruction, global object or argument, we don't care about this value
                            continue;
                        }
                    }
                    if (auto arg = dyn_cast<Argument>(op))
                    {
                        if (scopedFuncs.find(arg->getParent()) == scopedFuncs.end())
                        {
                            // we found an argument of the callinst that came from somewhere else
                            if (find(KernelImports.begin(), KernelImports.end(), GetValueID(arg)) == KernelImports.end())
                            {
                                //PrintVal(arg->getParent());
                                KernelImports.push_back(GetValueID(arg));
                            }
                        }
                    }
                    else if (auto *operand = dyn_cast<Instruction>(op))
                    {
                        BasicBlock *parentBlock = operand->getParent();
                        if (std::find(scopedBlocks.begin(), scopedBlocks.end(), parentBlock) == scopedBlocks.end())
                        {
                            if (find(KernelImports.begin(), KernelImports.end(), GetValueID(operand)) == KernelImports.end())
                            {
                                KernelImports.push_back(GetValueID(op));
                            }
                        }
                        // see if this value needs to be exported
                        else
                        {
                            for (auto &use : operand->uses())
                            {
                                if (auto useInst = dyn_cast<Instruction>(use))
                                {
                                    if (find(scopedBlocks.begin(), scopedBlocks.end(), useInst->getParent()) == scopedBlocks.end())
                                    {
                                        // we need to have an export for this value
                                        PrintVal(useInst);
                                    }
                                }
                                else if (auto useArg = dyn_cast<Argument>(use))
                                {
                                    if (find(scopedFuncs.begin(), scopedFuncs.end(), useArg->getParent()) == scopedFuncs.end())
                                    {
                                        // we need to have an export for this value
                                        PrintVal(useInst);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if (Entrances.empty())
        {
            throw AtlasException("Kernel Exception: tik requires a body entrance");
        }
    }

    void CartographerKernel::BuildKernelFromBlocks(llvm::ValueToValueMapTy &VMap, set<BasicBlock *> &blocks)
    {
        set<Function *> headFunctions;
        for (const auto &ent : Entrances)
        {
            BasicBlock *eTarget = IDToBlock[ent->Block];
            headFunctions.insert(eTarget->getParent());
        }

        if (headFunctions.size() != 1)
        {
            throw AtlasException("Entrances not on same level");
        }

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
                bool inNested = false;
                for (const auto &ent : nestedKernel->Entrances)
                {
                    if (IDToBlock[ent->Block] == block)
                    {
                        inNested = true;
                        break;
                    }
                }
                if (inNested)
                {
                    //we need to make a unique block for each entrance (there is currently only one)
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
                        SetBlockID(intermediateBlock, IDState::Artificial);
                        cc->setMetadata("KernelCall", tikNode);
                        auto sw = intBuilder.CreateSwitch(cc, Exception, (uint32_t)nestedKernel->Exits.size());
                        for (const auto &exit : nestedKernel->Exits)
                        {
                            sw->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t)exit->Index), IDToBlock[exit->Block]);
                        }
                        VMap[block] = intermediateBlock;
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
                                bool found = false;
                                for (const auto &ent : Entrances)
                                {
                                    if (IDToBlock[ent->Block] == block)
                                    {
                                        p->replaceIncomingBlockWith(pred, Init);
                                        rescheduled++;
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found)
                                {
                                    auto a = p->getBasicBlockIndex(pred);
                                    if (a >= 0)
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
                    if (auto ci = dyn_cast<CallBase>(bi))
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
            if (blocks.find(id) == blocks.end() && block != Exit && block != Init && block != Exception && id >= 0)
            {
                for (auto user : block->users())
                {
                    if (auto *phi = dyn_cast<PHINode>(user))
                    {
                        phi->removeIncomingValue(block);
                    }
                }
                bToRemove.push_back(block);
            }
        }
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
                                    auto subArg = IDToValue[subK->ArgumentMap[sarg]];
                                    if (subArg != nullptr)
                                    {
                                        if (IDToValue[subK->ArgumentMap[sarg]] == inst)
                                        {
                                            embeddedCallArgs[sarg] = inst;
                                        }
                                        else if (VMap[IDToValue[subK->ArgumentMap[sarg]]] == inst)
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
                                else if (VMap[IDToValue[subK->ArgumentMap[sarg]]] == IDToValue[ArgumentMap[arg]])
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
        for (const auto &ent : Entrances)
        {
            int64_t id = ent->Block;
            if (KernelMap.find(id) == KernelMap.end() && (VMap.find(IDToBlock[ent->Block]) != VMap.end()))
            {
                initSwitch->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), i), cast<BasicBlock>(VMap[IDToBlock[ent->Block]]));
            }
            else
            {
                throw AtlasException("Unimplemented");
            }
            i++;
        }
    }

    void CartographerKernel::BuildExit()
    {
        PrintVal(Exit, false); //another sacrifice
        IRBuilder<> exitBuilder(Exit);

        //start by getting the exits
        int exitId = 0;
        auto ex = GetExits(KernelFunction);
        map<BasicBlock *, BasicBlock *> exitMap;
        for (auto exit : ex)
        {
            Exits.insert(make_shared<KernelInterface>(exitId++, GetBlockID(exit)));
            BasicBlock *tmp = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
            IRBuilder<> builder(tmp);
            builder.CreateBr(Exit);
            exitMap[exit] = tmp;
        }

        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto block = cast<BasicBlock>(fi);
            auto term = block->getTerminator();
            if (term != nullptr)
            {
                for (uint32_t i = 0; i < term->getNumSuccessors(); i++)
                {
                    auto suc = term->getSuccessor(i);
                    if (suc->getParent() != KernelFunction)
                    {
                        //we have an exit
                        term->setSuccessor(i, exitMap[suc]);
                    }
                }
            }
        }

        auto phi = exitBuilder.CreatePHI(Type::getInt8Ty(TikModule->getContext()), (uint32_t)Exits.size());
        for (const auto &exit : Exits)
        {
            if (exitMap.find(IDToBlock[exit->Block]) == exitMap.end())
            {
                throw AtlasException("Block not found in Exit Map!");
            }
            phi->addIncoming(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t)exit->Index), exitMap[IDToBlock[exit->Block]]);
        }

        exitBuilder.CreateRet(phi);

        IRBuilder<> exceptionBuilder(Exception);
        exceptionBuilder.CreateRet(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t)IDState::Artificial));
    }

    void CartographerKernel::PatchPhis(ValueToValueMapTy &VMap)
    {
        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto b = cast<BasicBlock>(fi);

            vector<Instruction *> phisToRemove;
            for (auto &phi : b->phis())
            {

                // make sure our phi is accounting for all preds
                for (auto pred : predecessors(b))
                {
                    bool found = false;
                    for (unsigned int i = 0; i < phi.getNumIncomingValues(); i++)
                    {
                        if (phi.getIncomingBlock(i) == pred)
                        {
                            if (found)
                            {
                                // we have two values in here... take this one out
                                phi.removeIncomingValue(phi.getIncomingBlock(i), false);
                            }
                            found = true;
                        }
                    }
                    if (!found)
                    {
                        // this phi needs an entry from one of our kernel exits
                        // not clear yet which value should be exported. Probably a kernel export
                        PrintVal(&phi);
                    }
                }
                vector<BasicBlock *> valuesToRemove;
                for (uint32_t i = 0; i < phi.getNumIncomingValues(); i++)
                {
                    auto block = phi.getIncomingBlock(i);
                    BasicBlock *rblock;
                    if (VMap.find(phi.getIncomingBlock(i)) != VMap.end())
                    {
                        rblock = cast<BasicBlock>(VMap[phi.getIncomingBlock(i)]);
                    }
                    else
                    {
                        rblock = block;
                    }
                    if (block->getParent() != KernelFunction && rblock->getParent() != KernelFunction)
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
                            continue;
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
                    for (auto user : phi.users())
                    {
                        if (auto br = dyn_cast<BranchInst>(user))
                        {
                            if (br->isConditional())
                            {
                                auto b0 = br->getSuccessor(0);
                                auto b1 = br->getSuccessor(1);
                                if (b0 != b1)
                                {
                                    throw AtlasException("Phi successors don't match");
                                }
                                IRBuilder<> ib(br);
                                ib.CreateBr(b0);
                                phisToRemove.push_back(br);
                            }
                            else
                            {
                                throw AtlasException("Malformed phi user");
                            }
                        }
                        else
                        {
                            throw AtlasException("Unexpected phi user");
                        }
                    }
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

    void CartographerKernel::FixInvokes()
    {
        auto F = TikModule->getOrInsertFunction("__gxx_personality_v0", Type::getInt32Ty(TikModule->getContext()));
        for (auto &fi : *KernelFunction)
        {
            for (auto bi = fi.begin(); bi != fi.end(); bi++)
            {
                if (auto ii = dyn_cast<InvokeInst>(bi))
                {
                    auto a = ii->getLandingPadInst();
                    if (isa<BranchInst>(a))
                    {
                        auto unwind = ii->getUnwindDest();
                        auto term = unwind->getTerminator();
                        IRBuilder<> builder(term);
                        auto landing = builder.CreateLandingPad(Type::getVoidTy(TikModule->getContext()), 0);
                        landing->addClause(ConstantPointerNull::get(PointerType::get(Type::getVoidTy(TikModule->getContext()), 0)));
                        KernelFunction->setPersonalityFn(cast<Constant>(F.getCallee()));
                        spdlog::warn("Adding landingpad for non-inlinable Invoke Instruction. May segfault if exception is thrown.");
                    }
                    else
                    {
                        // will cause a "personality function from another module" module error
                        throw AtlasException("Could not deduce personality.")
                    }
                }
            }
        }
    }
} // namespace TraceAtlas::tik