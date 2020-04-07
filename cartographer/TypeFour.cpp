#include "TypeFour.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "cartographer.h"
#include <indicators/progress_bar.hpp>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <queue>
#include <spdlog/spdlog.h>

using namespace std;
using namespace llvm;

namespace TypeFour
{
    set<BasicBlock *> GetReachable(BasicBlock *base, set<int64_t> validBlocks)
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
                    if (f != nullptr && !f->empty())
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
            if (auto ri = dyn_cast<ReturnInst>(I))
            {
                Function *f = bb->getParent();
                for (auto use : f->users())
                {
                    if (auto cb = dyn_cast<CallBase>(use))
                    {
                        BasicBlock *entry = cb->getParent();
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
            else if (auto ri = dyn_cast<ResumeInst>(I))
            {
                Function *f = bb->getParent();
                for (auto use : f->users())
                {
                    if (auto cb = dyn_cast<CallBase>(use))
                    {
                        BasicBlock *entry = cb->getParent();
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
        }

        if (!foundSelf)
        {
            checked.erase(base);
        }
        return checked;
    }

    //this was taken from tik. It should probably be moved to libtik once it is made
    set<BasicBlock *> GetEntrances(set<BasicBlock *> &blocks)
    {
        set<BasicBlock *> Entrances;
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
        return Entrances;
    }

    bool IsSelfReachable(BasicBlock *base, set<int64_t> validBlocks)
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
                    if (f != nullptr && !f->empty())
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
            if (auto ri = dyn_cast<ReturnInst>(I))
            {
                Function *f = bb->getParent();
                for (auto use : f->users())
                {
                    if (auto cb = dyn_cast<CallBase>(use))
                    {
                        BasicBlock *entry = cb->getParent();
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
            else if (auto ri = dyn_cast<ResumeInst>(I))
            {
                Function *f = bb->getParent();
                for (auto use : f->users())
                {
                    if (auto cb = dyn_cast<CallBase>(use))
                    {
                        BasicBlock *entry = cb->getParent();
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
        }

        return foundSelf;
    }

    set<set<int64_t>> Process(const set<set<int64_t>> &type3Kernels)
    {
        indicators::ProgressBar bar;
        if (!noBar)
        {
            bar.set_prefix_text("Detecting type 4 kernels");
            bar.set_bar_width(50);
            bar.show_elapsed_time();
            bar.show_remaining_time();
        }

        uint64_t total = type3Kernels.size();
        int status = 0;

        set<set<int64_t>> result;
        for (const auto &kernel : type3Kernels)
        {
            set<int64_t> blocks;
            for (auto block : kernel)
            {
                //we need to see if this block can ever reach itself
                BasicBlock *base = blockMap[block];
                if (IsSelfReachable(base, kernel))
                {
                    blocks.insert(block);
                }
            }
            //blocks is now a set, but it may be disjoint, so we need to check that now

            set<BasicBlock *> blockSet;
            for (auto block : blocks)
            {
                blockSet.insert(blockMap[block]);
            }

            set<BasicBlock *> entrances = GetEntrances(blockSet);

            for (auto ent : entrances)
            {
                auto a = GetReachable(ent, blocks);
                set<int64_t> b;
                for (auto as : a)
                {
                    b.insert(GetBlockID(as));
                }
                result.insert(b);
            }
            status++;
            float percent = float(status) / float(total) * 100;
            bar.set_postfix_text("Kernel " + to_string(status) + "/" + to_string(total));
            bar.set_progress(percent);
        }

        if (!noBar && !bar.is_completed())
        {
            bar.set_postfix_text("Kernel " + to_string(status) + "/" + to_string(total));
            bar.set_progress(100);
            bar.mark_as_completed();
        }

        return result;
    }
} // namespace TypeFour