#include "Rectifier.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Print.h"
#include "cartographer.h"
#include <indicators/progress_bar.hpp>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <queue>
#include <spdlog/spdlog.h>

using namespace std;
using namespace llvm;

set<set<int>> RectifyKernel(set<set<int>> type3Kernels, Module *M)
{
    indicators::ProgressBar bar;
    if (!noProgressBar)
    {
        bar.set_prefix_text("Detecting type 4 kernels");
        bar.set_bar_width(50);
        bar.show_elapsed_time();
        bar.show_remaining_time();
    }

    int total = type3Kernels.size();
    int status = 0;

    set<set<int>> result;
    for (auto kernel : type3Kernels)
    {
        set<int> blocks;
        for (auto block : kernel)
        {
            //we need to see if this block can ever reach itself
            bool foundSelf = false;
            queue<BasicBlock *> toProcess;
            set<BasicBlock *> checked;
            BasicBlock *base = blockMap[block];
            toProcess.push(base);
            checked.insert(base);
            while (!toProcess.empty())
            {
                BasicBlock *bb = toProcess.front();
                toProcess.pop();
                for (auto suc : successors(bb))
                {
                    if (suc == base)
                    {
                        foundSelf = true;
                        break;
                    }
                    else
                    {
                        if (checked.find(suc) == checked.end())
                        {
                            int64_t id = GetBlockID(suc);
                            if (kernel.find(id) != kernel.end())
                            {
                                checked.insert(suc);
                                toProcess.push(suc);
                            }
                        }
                    }
                }
                //we now check if there is a function call, and if so add the entry
                for (auto bi = bb->begin(); bi != bb->end(); bi++)
                {
                    if (auto ci = dyn_cast<CallBase>(bi))
                    {
                        Function *f = ci->getCalledFunction();
                        if (f && !f->empty())
                        {
                            BasicBlock *entry = &f->getEntryBlock();
                            if (entry == base)
                            {
                                foundSelf = true;
                                break;
                            }
                            else
                            {
                                if (checked.find(entry) == checked.end())
                                {
                                    int64_t id = GetBlockID(entry);
                                    if (kernel.find(id) != kernel.end())
                                    {
                                        checked.insert(entry);
                                        toProcess.push(entry);
                                    }
                                }
                            }
                        }
                    }
                }
                //finally check the terminator and add the call points
                Instruction *I = bb->getTerminator();
                if (auto r = dyn_cast<ReturnInst>(I))
                {
                    for (auto user : r->getParent()->getParent()->users())
                    {
                        if (auto base = dyn_cast<CallBase>(user))
                        {
                            auto baseBlock = base->getParent();
                            if (checked.find(baseBlock) == checked.end())
                            {
                                int64_t id = GetBlockID(baseBlock);
                                if (kernel.find(id) != kernel.end())
                                {
                                    checked.insert(baseBlock);
                                    toProcess.push(baseBlock);
                                }
                            }
                        }
                    }
                }
                else if (auto r = dyn_cast<ResumeInst>(I))
                {
                    for (auto user : r->getParent()->getParent()->users())
                    {
                        if (auto base = dyn_cast<CallBase>(user))
                        {
                            auto baseBlock = base->getParent();
                            if (checked.find(baseBlock) == checked.end())
                            {
                                int64_t id = GetBlockID(baseBlock);
                                if (kernel.find(id) != kernel.end())
                                {
                                    checked.insert(baseBlock);
                                    toProcess.push(baseBlock);
                                }
                            }
                        }
                    }
                }
            }
            if (foundSelf)
            {
                //this is a valid block so add it to the block set
                blocks.insert(block);
            }
        }
        result.insert(blocks);
        status++;
        float percent = float(status) / float(total);
        bar.set_progress(percent);
    }

    if (!noProgressBar && !bar.is_completed())
    {
        bar.set_postfix_text("Kernel " + to_string(status) + "/" + to_string(total));
        bar.set_progress(100);
        bar.mark_as_completed();
    }

    return result;
}