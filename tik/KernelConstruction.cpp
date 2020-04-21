#include "tik/KernelConstruction.h"
#include "AtlasUtil/Exceptions.h"
#include <queue>
#include <llvm/IR/CFG.h>
#include <llvm/IR/IRBuilder.h>
#include "tik/tik.h"
#include "AtlasUtil/Annotate.h"
#include <llvm/Transforms/Utils/Cloning.h>
#include <spdlog/spdlog.h>
#include "tik/Metadata.h"

using namespace std;
using namespace llvm;

void GetEntrances(set<BasicBlock *> &blocks, set<llvm::BasicBlock *>& Entrances)
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

void GetExits(set<BasicBlock *> &blocks, std::map<int, llvm::BasicBlock *>& ExitTarget)
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
                    ExitTarget[exitId++] = suc;
                    coveredExits.insert(suc);
                    //ExitMap[block] = exitId++;
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
                        //ExitMap[block] = exitId++;
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

std::vector<llvm::Value *> GetExternalValues(set<BasicBlock *> &blocks, llvm::ValueToValueMapTy& VMap, std::vector<llvm::Value* >& KernelImports)
{
    std::vector<llvm::Value *> KernelExports;
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
    return KernelExports;
}

void BuildKernel(Function* KernelFunction, set<BasicBlock *> &blocks, set<llvm::BasicBlock *>& Conditional, set<BasicBlock*>& Entrances, BasicBlock* Exception, llvm::BasicBlock* Exit, std::map<llvm::BasicBlock *, int>& ExitMap, llvm::ValueToValueMapTy VMap, llvm::BasicBlock* Init)
{
    set<Function *> headFunctions;
    for (auto ent : Entrances)
    {
        //MDNode* oldID = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(ent->getParent()->getParent()->getContext()), ent->getValueID())));
        MDNode* oldID = MDNode::get(TikModule->getContext(), MDString::get(ent->getParent()->getParent()->getContext(), ent->getName()));// ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(ent->getParent()->getParent()->getContext()), ent->getValueID())));
        cast<Instruction>(ent->getFirstInsertionPt())->setMetadata("oldName", oldID);
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

void ApplyMetadata(Function* KernelFunction, set<llvm::BasicBlock *>& Conditional, string& Name, std::map<llvm::Value *, llvm::GlobalObject *>& GlobalMap)
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