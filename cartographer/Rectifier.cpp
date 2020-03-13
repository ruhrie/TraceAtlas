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

set<BasicBlock *> GetReachable(BasicBlock *base, set<int> validBlocks)
{
    bool foundSelf = false;
    queue<BasicBlock *> toProcess;
    set<BasicBlock *> checked;
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
            }
            if (checked.find(suc) == checked.end())
            {
                int64_t id = GetBlockID(suc);
                if (validBlocks.find(id) != validBlocks.end())
                {
                    checked.insert(suc);
                    toProcess.push(suc);
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
                    }
                    if (checked.find(entry) == checked.end())
                    {
                        int64_t id = GetBlockID(entry);
                        if (validBlocks.find(id) != validBlocks.end())
                        {
                            checked.insert(entry);
                            toProcess.push(entry);
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
                        if (validBlocks.find(id) != validBlocks.end())
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
                        if (validBlocks.find(id) != validBlocks.end())
                        {
                            checked.insert(baseBlock);
                            toProcess.push(baseBlock);
                        }
                    }
                }
            }
        }
    }

    if (!foundSelf)
    {
        checked.erase(base);
    }
    return checked;
}

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
        map<int, set<BasicBlock *>> reachableMap;
        for (auto block : kernel)
        {
            //we need to see if this block can ever reach itself
            BasicBlock *base = blockMap[block];
            auto reachable = GetReachable(base, kernel);
            if (reachable.find(base) != reachable.end())
            {
                blocks.insert(block);
            }
            reachableMap[block] = reachable;
        }
        //blocks is now a set, but it may be disjoint, so we need to check that now
        map<int, set<BasicBlock *>> reachableBlockSets;
        for (auto block : blocks)
        {
            reachableBlockSets[block] = GetReachable(blockMap[block], blocks);
        }
        set<set<int>> subSets;
        for (auto block : blocks)
        {
            set<int> sub;
            for (auto a : reachableBlockSets[block])
            {
                int64_t id = GetBlockID(a);
                if (reachableBlockSets[id].find(blockMap[block]) != reachableBlockSets[id].end())
                {
                    sub.insert(id);
                }
            }
            subSets.insert(sub);
        }
        for (auto subSet : subSets)
        {
            result.insert(subSet);
        }
        status++;
        float percent = float(status) / float(total) * 100;
        bar.set_postfix_text("Kernel " + to_string(status) + "/" + to_string(total));
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