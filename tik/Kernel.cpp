#include "tik/Kernel.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "tik/InlineStruct.h"
#include "tik/Metadata.h"
#include "tik/TikHeader.h"
#include "tik/Util.h"
#include "tik/tik.h"
#include <algorithm>
#include <iostream>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Comdat.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <queue>
#include <spdlog/spdlog.h>

using namespace llvm;
using namespace std;

static int KernelUID = 0;

set<string> reservedNames;
std::map<llvm::GlobalVariable *, llvm::GlobalVariable *> globalDeclaractionMap;

Kernel::Kernel(std::vector<int64_t> basicBlocks, Module *M, string name)
{
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

        GetEntrances(blocks);
        //GetExits(blocks);
        GetExternalValues(blocks);

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
        BuildKernel(blocks);

        Remap(); //we need to remap before inlining

        InlineFunctions(blockSet);

        CopyGlobals();

        //remap and repipe
        Remap();
        //might be fused
        Repipe();

        // replace external function calls with tik declarations
        ExportFunctionSignatures();

        //handle the memory operations
        //GetMemoryFunctions();

        //UpdateMemory();

        BuildInit();

        BuildExit();

        RemapNestedKernels();

        RemapExports();

        PatchPhis();

        //apply metadata
        ApplyMetadata();

        //and set a flag that we succeeded
        Valid = true;
    }
    catch (AtlasException &e)
    {
        spdlog::error(e.what());
        Cleanup();
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

void Kernel::Cleanup()
{
    if (KernelFunction != nullptr)
    {
        KernelFunction->eraseFromParent();
    }
    /*
    if (MemoryRead)
    {
        MemoryRead->eraseFromParent();
    }
    if (MemoryWrite)
    {
        MemoryWrite->eraseFromParent();
    }
    for (auto g : GlobalMap)
    {
        g.second->eraseFromParent();
    }
    */
}

Kernel::~Kernel() = default;

void Kernel::ExportFunctionSignatures()
{
    for (auto &bi : *KernelFunction)
    {
        for (auto inst = bi.begin(); inst != bi.end(); inst++)
        {
            if (auto *callBase = dyn_cast<CallBase>(inst))
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
}

void Kernel::UpdateMemory()
{
    std::set<llvm::Value *> coveredGlobals;
    std::set<llvm::StoreInst *> newStores;
    IRBuilder<> initBuilder(Init);
    for (uint64_t i = 0; i < KernelImports.size(); i++)
    {
        if (GlobalMap.find(VMap[KernelImports[i]]) != GlobalMap.end())
        {
            if (GlobalMap[VMap[KernelImports[i]]] == nullptr)
            {
                throw AtlasException("Tik Error: External Value not found in GlobalMap.");
            }
            coveredGlobals.insert(GlobalMap[VMap[KernelImports[i]]]);
            auto b = initBuilder.CreateStore(KernelFunction->arg_begin() + i + 1, GlobalMap[VMap[KernelImports[i]]]);
            MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Store))));
            b->setMetadata("TikSynthetic", tikNode);
            newStores.insert(b);
        }
    }

    for (auto &bb : *KernelFunction)
    {
        for (BasicBlock::iterator BI = bb.begin(), BE = bb.end(); BI != BE; ++BI)
        {
            auto *inst = cast<Instruction>(BI);
            for (auto pair : GlobalMap)
            {
                if (pair.first == inst)
                {
                    if (coveredGlobals.find(pair.second) == coveredGlobals.end())
                    {
                        if (isa<InvokeInst>(inst))
                        {
                            throw AtlasException("Invoke is unsupported");
                        }
                        IRBuilder<> builder(inst->getNextNode());
                        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
                        auto a = builder.CreateGEP(inst->getType(), pair.second, constant);
                        auto b = builder.CreateStore(inst, a);
                        MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Store))));
                        b->setMetadata("TikSynthetic", tikNode);
                    }
                }
            }
        }
    }
}
/*
void Kernel::RemapOperand(llvm::Operator* op)
{
    for(int i = 0; i < op->getNumOperands(); i++)
    {
        llvm::Value* val = op->getOperand(i);
        if (Operator* embeddedOp = dyn_cast<Operator>(val))
        {
            RemapOperand(embeddedOp);
        }
        else
        {
            if (llvm::Constant* con = dyn_cast<llvm::Constant>(val))
            {
                // we have to reconstruct the operator. 
                if (llvm::GEPOperator* gep = dyn_cast<llvm::GEPOperator>(op))
                {
                    for (auto index = gep->idx_begin(), end = gep->idx_end(); index != end; index++)
                    {
                        PrintVal(index);
                    }
                    static llvm::Constant* newGep = llvm::ConstantExpr::getGetElementPtr(gep->getType(), con, );
                }
                //auto uses = op->getOperandUse(i);
                //llvm::Instruction* inst = dyn_cast<Instruction>(uses);
                PrintVal(val);
                continue;
            }
            if (VMap[val])
            {
                op->setOperand(i, VMap[val]);
            }
            else
            {
                spdlog::warn("Could not remap value in operand.");
            }
        }
    }
}
*/
void Kernel::Remap()
{
    for (auto &BB : *KernelFunction)
    {
        for (BasicBlock::iterator BI = BB.begin(), BE = BB.end(); BI != BE; ++BI)
        {
            auto *inst = cast<Instruction>(BI);
            /*if (llvm::Operator* op = dyn_cast<Operator>(inst))
            {
                RemapOperand(op);
            }*/
            RemapInstruction(inst, VMap, llvm::RF_None);
        }
    }
}

void Kernel::RemapNestedKernels()
{
    // look through the body for pointer references
    // every time we see a global reference who is not written to in MemRead, not stored to in Init, store to it in body

    // every time we use a global pointer in the body that is not in MemWrite, store to it

    // loop->body or loop->exit (conditional)
    // body->loop (unconditional)

    // Now find all calls to the embedded kernel functions in the body, if any, and change their arguments to the new ones
    std::map<Argument *, Value *> embeddedCallArgs;
    for (auto &bf : *KernelFunction)
    {
        for (BasicBlock::iterator i = bf.begin(), BE = bf.end(); i != BE; ++i)
        {
            if (auto *callInst = dyn_cast<CallInst>(i))
            {
                Function *funcCal = callInst->getCalledFunction();
                llvm::Function *funcName = TikModule->getFunction(funcCal->getName());
                if (funcName == nullptr)
                {
                    // we have a non-kernel function call
                }
                //else if (funcName != MemoryRead && funcName != MemoryWrite) // must be a kernel function call
                //{
                auto calledFunc = callInst->getCalledFunction();
                auto subK = KfMap[calledFunc];
                if (subK != nullptr)
                {
                    for (auto sarg = calledFunc->arg_begin(); sarg < calledFunc->arg_end(); sarg++)
                    {
                        for (auto &b : *KernelFunction)
                        {
                            for (BasicBlock::iterator j = b.begin(), BE2 = b.end(); j != BE2; ++j)
                            {
                                if (subK->ArgumentMap[sarg] == cast<Instruction>(j))
                                {
                                    embeddedCallArgs[sarg] = cast<Instruction>(j);
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
                        }
                    }
                    auto limit = callInst->getNumArgOperands();
                    for (uint32_t k = 0; k < limit; k++)
                    {
                        Value *op = callInst->getArgOperand(k);
                        if (auto *arg = dyn_cast<Argument>(op))
                        {
                            auto asdf = embeddedCallArgs[arg];
                            callInst->setArgOperand(k, asdf);
                        }
                        else if (auto *c = dyn_cast<Constant>(op))
                        {
                            //we don't have to do anything so ignore
                        }
                        else
                        {
                            throw AtlasException("Tik Error: Unexpected value passed to function");
                        }
                    }
                }
                //}
            }
        }
    }
}

void Kernel::BuildInit()
{
    IRBuilder<> initBuilder(Init);
    auto initSwitch = initBuilder.CreateSwitch(KernelFunction->arg_begin(), Exception, (uint32_t)Entrances.size());
    uint64_t i = 0;
    for (auto ent : Entrances)
    {
        initSwitch->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), i), cast<BasicBlock>(VMap[ent]));
        i++;
    }
}

void Kernel::BuildKernel(set<BasicBlock *> &blocks)
{
    set<Function *> headFunctions;
    for (auto ent : Entrances)
    {
        headFunctions.insert(ent->getParent());
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
            if (nestedKernel->Entrances.find(block) != nestedKernel->Entrances.end())
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
                                    throw AtlasException("Expected exactly one exit fron nested kernel");
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
                            if (Entrances.find(block) != Entrances.end())
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

void Kernel::GetExternalValues(set<BasicBlock *> &blocks)
{
    for (auto bb : blocks)
    {
        for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
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
                                Value *v = VMap[sExtVal];
                                if (v == nullptr)
                                {
                                    if (find(KernelImports.begin(), KernelImports.end(), sExtVal) == KernelImports.end())
                                    {
                                        KernelImports.push_back(sExtVal);
                                    }
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
}

void Kernel::BuildExit()
{
    PrintVal(Exit, false); //another sacrifice
    IRBuilder<> exitBuilder(Exit);
    int i = 0;
    for (auto pred : predecessors(Exit))
    {
        ExitMap[pred] = i++;
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

//if the result is one entry long it is a value. Otherwise its a list of instructions
vector<Value *> Kernel::BuildReturnTree(BasicBlock *bb, vector<BasicBlock *> blocks)
{
    vector<Value *> result;
    Instruction *term = bb->getTerminator();
    if (auto *retInst = dyn_cast<ReturnInst>(term))
    {
        //so the block is a return, just return the value
        result.push_back(retInst->getReturnValue());
    }
    else if (auto *brInst = dyn_cast<BranchInst>(term))
    {
        //we have a branch, if it is unconditional recurse on the target
        //otherwise recurse on all targets and build a select between the results
        //we assume the last entry in the vector is the result to be selected, so be careful on the order of insertion
        if (brInst->isConditional())
        {
            //we have a conditional so select between return vals
            //still need to check if successors are valid though
            Value *cond = brInst->getCondition();
            auto suc0 = brInst->getSuccessor(0);
            auto suc1 = brInst->getSuccessor(1);
            bool valid0 = find(blocks.begin(), blocks.end(), suc0) != blocks.end();
            bool valid1 = find(blocks.begin(), blocks.end(), suc1) != blocks.end();
            if (!(valid0 || valid1))
            {
                throw AtlasException("Tik Error: Branch instruction with no valid successors reached");
            }
            Value *c0 = nullptr;
            Value *c1 = nullptr;
            if (valid0) //if path 0 is valid we examine it
            {
                auto sub0 = BuildReturnTree(suc0, blocks);
                if (sub0.size() != 1)
                {
                    //we can just copy them
                    //otherwise this is a single value and should just be referenced
                    result.insert(result.end(), sub0.begin(), sub0.end());
                }
                c0 = sub0.back();
            }
            if (valid1) //if path 1 is valid we examine it
            {
                auto sub1 = BuildReturnTree(brInst->getSuccessor(1), blocks);
                if (sub1.size() != 1)
                {
                    //we can just copy them
                    //otherwise this is a single value and should just be referenced
                    result.insert(result.end(), sub1.begin(), sub1.end());
                }
                c1 = sub1.back();
            }
            if (valid0 && valid1) //if they are both valid we need to select between them
            {
                SelectInst *sInst = SelectInst::Create(cond, c0, c1);
                result.push_back(sInst);
            }
        }
        else
        {
            //we are not conditional so just recursef
            auto sub = BuildReturnTree(brInst->getSuccessor(0), blocks);
            result.insert(result.end(), sub.begin(), sub.end());
        }
    }
    else
    {
        throw AtlasException("Tik Error: Not Implemented");
    }
    if (result.empty())
    {
        throw AtlasException("Tik Error: Return instruction tree must have at least one result");
    }
    return result;
}

void Kernel::ApplyMetadata()
{
    //first we will clean the current instructions
    for (auto &fi : *KernelFunction)
    {
        for (auto bi = fi.begin(); bi != fi.end(); bi++)
        {
            auto *inst = cast<Instruction>(bi);
            inst->setMetadata("dbg", nullptr);
        }
    }

    //second remove all debug intrinsics
    vector<Instruction *> toRemove;
    for (auto &fi : *KernelFunction)
    {
        for (auto bi = fi.begin(); bi != fi.end(); bi++)
        {
            if (auto di = dyn_cast<DbgInfoIntrinsic>(bi))
            {
                toRemove.push_back(di);
            }
            auto *inst = cast<Instruction>(bi);
            inst->setMetadata("dbg", nullptr);
        }
    }
    for (auto r : toRemove)
    {
        r->eraseFromParent();
    }

    //annotate the kernel functions
    MDNode *kernelNode = MDNode::get(TikModule->getContext(), MDString::get(TikModule->getContext(), Name));
    KernelFunction->setMetadata("KernelName", kernelNode);
    //MemoryRead->setMetadata("KernelName", kernelNode);
    //MemoryWrite->setMetadata("KernelName", kernelNode);
    for (auto global : GlobalMap)
    {
        global.second->setMetadata("KernelName", kernelNode);
    }
    //annotate the conditional, has to happen after body since conditional is a part of the body
    MDNode *condNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikMetadata::Conditional))));
    for (auto cond : Conditional)
    {
        cast<Instruction>(cond->getFirstInsertionPt())->setMetadata("TikMetadata", condNode);
    }
}

void Kernel::Repipe()
{
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
}

void Kernel::GetEntrances(set<BasicBlock *> &blocks)
{
    for (auto block : blocks)
    {
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
                Entrances.insert(block);
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
                Entrances.insert(block);
            }
        }
    }
    if (Entrances.empty())
    {
        throw AtlasException("Kernel Exception: tik requires a body entrance");
    }
}

void Kernel::SanityChecks()
{
    for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
    {
        auto *BB = cast<BasicBlock>(fi);
        auto predIter = predecessors(BB);
        int64_t predCount = distance(predIter.begin(), predIter.end());
        if (predCount == 0)
        {
            if (BB != Init)
            {
                throw AtlasException("Tik Sanity Failure: No predecessors");
            }
        }
    }
}

void Kernel::GetExits(set<BasicBlock *> &blocks)
{
    int exitId = 0;
    // search for exit basic blocks
    set<BasicBlock *> coveredExits;
    for (auto block : blocks)
    {
        for (auto suc : successors(block))
        {
            if (blocks.find(suc) == blocks.end())
            {
                if (coveredExits.find(suc) == coveredExits.end())
                {
                    ExitTarget[exitId] = suc;
                    coveredExits.insert(suc);
                    ExitMap[block] = exitId++;
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
                        ExitTarget[exitId] = v->getParent();
                        coveredExits.insert(v->getParent());
                        ExitMap[block] = exitId++;
                    }
                }
            }
        }
    }
    if (exitId == 0)
    {
        throw AtlasException("Tik Error: tik found no kernel exits")
    }
    if (exitId != 1)
    {
        //removing this is exposing an llvm bug: corrupted double-linked list
        //we just won't support it for the moment
        //throw AtlasException("Tik Error: tik only supports single exit kernels")
    }
}

void Kernel::CopyGlobals()
{
    for (auto &fi : *KernelFunction)
    {
        for (auto bi = fi.begin(); bi != fi.end(); bi++)
        {
            auto *inst = cast<Instruction>(bi);
            if (auto *cv = dyn_cast<CallBase>(inst))
            {
                CopyArgument(cv);
            }
            else
            {
                CopyOperand(inst);
            }
        }
    }
}

std::string Kernel::GetHeaderDeclaration(std::set<llvm::StructType *> &AllStructures)
{
    std::string headerString;
    try
    {
        headerString = getCType(KernelFunction->getReturnType(), AllStructures) + " ";
    }
    catch (AtlasException &e)
    {
        spdlog::error(e.what());
        headerString = "TypeNotSupported ";
    }
    headerString += KernelFunction->getName();
    headerString += "(";
    int i = 0;
    for (auto ai = KernelFunction->arg_begin(); ai < KernelFunction->arg_end(); ai++)
    {
        std::string type;
        std::string argname = "arg" + std::to_string(i);
        if (i > 0)
        {
            headerString += ", ";
        }
        try
        {
            type = getCType(ai->getType(), AllStructures);
        }
        catch (AtlasException &e)
        {
            spdlog::error(e.what());
            type = "TypeNotSupported";
        }
        if (type.find('!') != std::string::npos)
        {
            ProcessArrayArgument(type, argname);
        }
        else if (type.find('@') != std::string::npos)
        {
            ProcessFunctionArgument(type, argname);
        }
        else
        {
            type += " " + argname;
        }
        headerString += type;
        i++;
    }
    headerString += ");\n";
    return headerString;
}

void Kernel::CopyArgument(llvm::CallBase *Call)
{
    for (auto i = Call->arg_begin(); i < Call->arg_end(); i++)
    {
        // if we are a global, copy it
        if (auto *gv = dyn_cast<GlobalVariable>(i))
        {
            Module *m = gv->getParent();
            if (m != TikModule)
            {
                //its the wrong module
                if (gv->getParent() != TikModule)
                {
                    if (gv->hasInitializer())
                    {
                        llvm::Constant *value = gv->getInitializer();
                        for (uint32_t iter = 0; iter < value->getNumOperands(); iter++)
                        {
                            auto *internal = cast<llvm::User>(value->getOperand(iter));
                            CopyOperand(internal);
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
                    globalDeclaractionMap[gv] = newGlobal;
                    VMap[gv] = newGlobal;
                    for (auto user : gv->users())
                    {
                        if (auto *inst = dyn_cast<llvm::Instruction>(user))
                        {
                            if (inst->getModule() == TikModule)
                            {
                                user->replaceUsesOfWith(gv, newGlobal);
                            }
                        }
                    }
                }
            }
        }
        // if we have a GEP as a function arg, get its pointer arg
        else if (auto *gop = dyn_cast<llvm::GEPOperator>(i))
        {
            CopyOperand(gop);
        }
        /*// if we are anything else, we don't know what to do
        else if (GlobalValue *gv = dyn_cast<GlobalValue>(i))
        {
            spdlog::warn("Non variable global reference"); //basically this is a band aid. Needs some more help
        }
        else if (llvm::BitCastOperator* op = dyn_cast<llvm::BitCastOperator>(i))
        {
            CopyOperand(op);
            std::cout << "This is a bitcast operator." << std::endl;
        }
        else if (llvm::PtrToIntOperator* op = dyn_cast<llvm::PtrToIntOperator>(i))
        {
            std::cout << "This is a PtrToInt operator." << std::endl;
        }
        else if (llvm::ZExtOperator* op = dyn_cast<llvm::ZExtOperator>(i))
        {
            std::cout << "This is a Zext operator." << std::endl;
        }
        else if (llvm::FPMathOperator* op = dyn_cast<llvm::FPMathOperator>(i))
        {
            CopyOperand(op);
            std::cout << "This is an FPMath operator." << std::endl;
        }
        else if (llvm::OverflowingBinaryOperator* op = dyn_cast<llvm::OverflowingBinaryOperator>(i))
        {
            std::cout << "This is an OF binary operator." << std::endl;
        }
        else if (llvm::PossiblyExactOperator* op = dyn_cast<llvm::PossiblyExactOperator>(i))
        {
            std::cout << "This is a possibly exact operator." << std::endl;
        }*/
        else if (isa<Operator>(i))
        {
            spdlog::warn("Function argument operand type not supported for global copying."); //basically this is a band aid. Needs some more help
        }
    }
}

void Kernel::CopyOperand(llvm::User *inst)
{
    for (uint32_t j = 0; j < inst->getNumOperands(); j++)
    {
        Value *v = inst->getOperand(j);
        if (auto *gv = dyn_cast<GlobalVariable>(v))
        {
            Module *m = gv->getParent();
            if (m != TikModule)
            {
                //its the wrong module
                if (globalDeclaractionMap.find(gv) == globalDeclaractionMap.end())
                {
                    // iterate through all internal operators of this global
                    PrintVal(gv);
                    if (gv->hasInitializer())
                    {
                        llvm::Constant *value = gv->getInitializer();
                        for (uint32_t iter = 0; iter < value->getNumOperands(); iter++)
                        {
                            auto *internal = cast<llvm::User>(value->getOperand(iter));
                            CopyOperand(internal);
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
                    globalDeclaractionMap[gv] = newGlobal;
                    VMap[gv] = newGlobal;
                    for (auto user : gv->users())
                    {
                        if (auto *inst = dyn_cast<llvm::Instruction>(user))
                        {
                            if (inst->getModule() == TikModule)
                            {
                                user->replaceUsesOfWith(gv, newGlobal);
                            }
                        }
                    }
                }
            }
        }
        else if (auto *gv = dyn_cast<GlobalValue>(v))
        {
            throw AtlasException("Tik Error: Non variable global reference");
        }
        else if (auto *con = dyn_cast<Constant>(v))
        {
        }
        else
        {
        }
    }
}

void Kernel::InlineFunctions(set<int64_t> &blocks)
{
    bool change = true;
    while (change)
    {
        change = false;
        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto baseBlock = cast<BasicBlock>(fi);
            for (auto bi = fi->begin(); bi != fi->end(); bi++)
            {
                if (auto *ci = dyn_cast<CallInst>(bi))
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
        if (auto *b = dyn_cast<BasicBlock>(fi))
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
        auto *block = cast<BasicBlock>(fi);
        int64_t id = GetBlockID(block);
        if (blocks.find(id) == blocks.end() && block != Exit && block != Init && block != Exception)
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
    for (auto block : bToRemove)
    {
        block->eraseFromParent();
    }
}

void Kernel::RemapExports()
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
        auto *block = cast<BasicBlock>(fi);
        for (auto bi = fi->begin(); bi != fi->end(); bi++)
        {
            auto *i = cast<Instruction>(bi);
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

void Kernel::PatchPhis()
{
    for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
    {
        auto *b = cast<BasicBlock>(fi);
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