#include "Smoothing.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>

using namespace std;
using namespace llvm;

std::map<int, set<int>> SmoothKernel(std::map<int, std::set<int>> blocks, string bitcodeFile)
{
    std::map<int, set<int>> result;
    LLVMContext context;
    SMDiagnostic smerror;
    unique_ptr<Module> sourceBitcode = parseIRFile(bitcodeFile, smerror, context);

    static uint64_t UID = 0;
    for (Module::iterator F = sourceBitcode->begin(), E = sourceBitcode->end(); F != E; ++F)
    {
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            BB->setName("BB_UID_" + std::to_string(UID++));
        }
    }

    for (auto pair : blocks)
    {
        int index = pair.first;
        set<int> blk = pair.second;
        set<BasicBlock *> bbs;
        set<BasicBlock *> toRemove;
        for (Module::iterator F = sourceBitcode->begin(), E = sourceBitcode->end(); F != E; ++F)
        {
            for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
            {
                uint64_t id = std::stoul(BB->getName().substr(7));
                if (blk.find(id) != blk.end())
                {
                    bbs.insert(cast<BasicBlock>(BB));
                }
            }
        }

        bool change = true;

        while (change)
        {
            change = false;

            for (auto block : bbs)
            {
                //this is each block in the kernel
                //we need to check the entrance and exits
                //entrance first
                bool valid = true;
                bool entPresent = false;
                for (BasicBlock *pred : predecessors(block))
                {
                    if (bbs.find(pred) != bbs.end())
                    {
                        entPresent = true;
                        break;
                    }
                }
                //if we didn't find it we are either the first block of a function or aren't included
                if (!entPresent)
                {
                    Function *f = block->getParent();
                    if (&(f->getEntryBlock()) == block)
                    {
                        //it is an entry block so it may still be valid
                        //we now need to check if there are any calls to this function in the code
                        bool called = false;
                        for (auto block2 : bbs)
                        {
                            for (auto bi = block2->begin(); bi != block2->end(); bi++)
                            {
                                if (CallInst *ci = dyn_cast<CallInst>(bi))
                                {
                                    if (ci->getCalledFunction() == f)
                                    {
                                        //we do call so we do add it
                                        called = true;
                                        break;
                                    }
                                }
                            }
                        }
                        if (!called)
                        {
                            valid = false;
                        }
                    }
                }

                //then exit
                Instruction *term = block->getTerminator();
                if (ReturnInst *r = dyn_cast<ReturnInst>(term))
                {
                    BasicBlock *b = &(r->getParent()->getParent()->getEntryBlock());
                    if (bbs.find(b) == bbs.end())
                    {
                        //it should not be there
                        valid = false;
                    }
                }
                else if (BranchInst *br = dyn_cast<BranchInst>(term))
                {
                    bool found = false;
                    int s = br->getNumSuccessors();
                    for (int i = 0; i < s; i++)
                    {
                        BasicBlock *ss = br->getSuccessor(i);
                        if (bbs.find(ss) != bbs.end())
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        valid = false;
                    }
                }
                else if (SwitchInst *sw = dyn_cast<SwitchInst>(term))
                {
                    bool found = false;
                    int s = sw->getNumSuccessors();
                    for (int i = 0; i < s; i++)
                    {
                        BasicBlock *ss = sw->getSuccessor(i);
                        if (bbs.find(ss) != bbs.end())
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        valid = false;
                    }
                }
                else if (IndirectBrInst *ibr = dyn_cast<IndirectBrInst>(term))
                {
                    bool found = false;
                    int s = ibr->getNumSuccessors();
                    for (int i = 0; i < s; i++)
                    {
                        BasicBlock *ss = ibr->getSuccessor(i);
                        if (bbs.find(ss) != bbs.end())
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        valid = false;
                    }
                }
                else if (InvokeInst *iv = dyn_cast<InvokeInst>(term))
                {
                    bool found = false;
                    if (bbs.find(iv->getNormalDest()) != bbs.end())
                    {
                        found = true;
                    }
                    else if (bbs.find(iv->getUnwindDest()) != bbs.end())
                    {
                        found = true;
                    }
                    if (!found)
                    {
                        valid = false;
                    }
                }
                else if (CallBrInst *cb = dyn_cast<CallBrInst>(term))
                {
                    BasicBlock *b = &(cb->getCalledFunction()->getEntryBlock());
                    if (bbs.find(b) == bbs.end())
                    {
                        valid = false;
                    }
                }
                else if (ResumeInst *ri = dyn_cast<ResumeInst>(term))
                {
                    //don't know what to do with this so we won't prune here
                }
                else if (CatchSwitchInst *cw = dyn_cast<CatchSwitchInst>(term))
                {
                    bool found = false;
                    int s = cw->getNumSuccessors();
                    for (int i = 0; i < s; i++)
                    {
                        BasicBlock *ss = cw->getSuccessor(i);
                        if (bbs.find(ss) != bbs.end())
                        {
                            found = true;
                            break;
                        }
                    }
                    if (bbs.find(cw->getUnwindDest()) != bbs.end())
                    {
                        found = true;
                    }
                    if (!found)
                    {
                        valid = false;
                    }
                }
                else if (CatchReturnInst *cr = dyn_cast<CatchReturnInst>(term))
                {
                    if (bbs.find(cr->getSuccessor()) == bbs.end())
                    {
                        valid = false;
                    }
                }
                else if (CleanupReturnInst *cp = dyn_cast<CleanupReturnInst>(term))
                {
                    if (bbs.find(cp->getUnwindDest()) == bbs.end())
                    {
                        valid = false;
                    }
                }
                else if (UnreachableInst *ur = dyn_cast<UnreachableInst>(term))
                {
                    //we dont add these
                    valid = false;
                }

                //now we add
                if (!valid)
                {
                    toRemove.insert(block);
                }
            }
            if(toRemove.size() != 0)
            {
                change = true;
                for(auto b : toRemove)
                {
                    bbs.erase(bbs.find(b));
                }
            }
        }

        set<int> preR;
        for (auto b : bbs)
        {
            preR.insert(std::stoul(b->getName().substr(7)));
        }

        result[index] = preR;
    }

    return result;
}
