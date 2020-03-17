#include "TypeThree.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Print.h"
#include "cartographer.h"
#include <indicators/progress_bar.hpp>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <spdlog/spdlog.h>
using namespace std;
using namespace llvm;

namespace TypeThree
{
    std::set<set<int>> Process(std::set<std::set<int>> blocks, Module *M)
    {
        indicators::ProgressBar bar;
        if (!noBar)
        {
            bar.set_prefix_text("Detecting type 3 kernels");
            bar.set_bar_width(50);
            bar.show_elapsed_time();
            bar.show_remaining_time();
        }

        int status = 0;
        int total = blocks.size();

        set<set<int>> result;

        float percent;

        for (const auto &blk : blocks)
        {
            percent = (float)status / (float)total * 100;
            if (!noBar)
            {
                bar.set_progress(percent);
            }
            //for every kernel do this
            set<BasicBlock *> bbs;
            set<BasicBlock *> toRemove;
            for (Module::iterator F = M->begin(), E = M->end(); F != E; ++F)
            {
                for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
                {
                    int64_t id = GetBlockID(cast<BasicBlock>(BB));
                    if (blk.find(id) != blk.end())
                    {
                        bbs.insert(cast<BasicBlock>(BB));
                    }
                }
            }
            bool change = true;
            int trimCount = 0;
            int totalCount = bbs.size();
            while (change)
            {
                change = false;

                for (auto &block : bbs)
                {
                    int id = GetBlockID(block);
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
                        bool called = false;
                        if (&(f->getEntryBlock()) == block)
                        {
                            //it is an entry block so it may still be valid
                            //we now need to check if there are any calls to this function in the code
                            for (auto user : f->users())
                            {
                                if (auto ui = dyn_cast<CallInst>(user))
                                {
                                    BasicBlock *par = ui->getParent();
                                    if (bbs.find(par) != bbs.end())
                                    {
                                        called = true;
                                        break;
                                    }
                                }
                                else if (auto ui = dyn_cast<InvokeInst>(user))
                                {
                                    BasicBlock *par = ui->getParent();
                                    if (bbs.find(par) != bbs.end())
                                    {
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

                    //then exit
                    Instruction *term = block->getTerminator();
                    if (ReturnInst *r = dyn_cast<ReturnInst>(term))
                    {
                        bool called = false;
                        Function *f = r->getParent()->getParent();
                        for (auto user : f->users())
                        {
                            if (auto ui = dyn_cast<CallInst>(user))
                            {
                                BasicBlock *par = ui->getParent();
                                if (bbs.find(par) != bbs.end())
                                {
                                    called = true;
                                    break;
                                }
                            }
                            else if (auto ui = dyn_cast<InvokeInst>(user))
                            {
                                BasicBlock *par = ui->getParent();
                                if (bbs.find(par) != bbs.end())
                                {
                                    called = true;
                                    break;
                                }
                            }
                        }
                        if (!called)
                        {
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
                if (toRemove.size() != 0)
                {
                    change = true;
                    for (auto b : toRemove)
                    {
                        bbs.erase(b);
                    }
                    trimCount += toRemove.size();
                    if (!noBar)
                    {
                        bar.set_postfix_text("Trimmed " + to_string(trimCount) + "/" + to_string(totalCount) + " blocks, Kernel " + to_string(status) + "/" + to_string(total));
                        bar.set_progress(percent);
                    }
                    toRemove.clear();
                }
            }

            set<int> preR;
            for (auto b : bbs)
            {
                int64_t id = GetBlockID(b);
                preR.insert(id);
            }
            if (!preR.empty())
            {
                result.insert(preR);
            }
            status++;
        }

        if (!noBar && !bar.is_completed())
        {
            bar.set_postfix_text("Kernel " + to_string(status) + "/" + to_string(total));
            bar.set_progress(100);
            bar.mark_as_completed();
        }

        return result;
    }
} // namespace TypeThree
