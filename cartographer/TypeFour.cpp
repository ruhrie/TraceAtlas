#include "TypeFour.h"
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

namespace TypeFour
{
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

    set<set<int>> Process(set<set<int>> type3Kernels, Module *M)
    {
        indicators::ProgressBar bar;
        if (!noBar)
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
            set<set<BasicBlock *>> subSets;
            for (auto block : blocks)
            {
                set<BasicBlock *> sub;
                for (auto a : reachableBlockSets[block])
                {
                    int64_t id = GetBlockID(a);
                    if (reachableBlockSets[id].find(blockMap[block]) != reachableBlockSets[id].end())
                    {
                        sub.insert(a);
                    }
                }
                subSets.insert(sub);
            }

            //we need to do a final split based upon the entrances
            for (auto subSet : subSets)
            {
                auto entrances = GetEntrances(subSet);
                for (auto entrance : entrances)
                {
                    //we will basically follow the path of the computation
                    //adding all blocks that are valid
                    auto visit = GetVisitable(entrance, subSet);
                    set<int> fin;
                    for (auto v : visit)
                    {
                        fin.insert(GetBlockID(v));
                    }
                    result.insert(fin);
                }
                //result.insert(subSet);
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

    //stolen from tik
    set<BasicBlock *> GetEntrances(set<BasicBlock *> blocks)
    {
        set<BasicBlock *> result;
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
                    result.insert(block);
                }
                else if (exte && inte)
                {
                    //both external and internal, so maybe an entrance
                    //ignore for the moment, worst case we will have no entrances
                    //throw TikException("Mixed function uses, not implemented");
                }
                else if (!exte && inte)
                {
                    //only internal, so ignore
                }
                else
                {
                    //neither internal or external, throw error
                    //throw TikException("Function with no internal or external uses");
                }
            }
            else
            {
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
                        ent = true;
                        break;
                    }
                    for (BasicBlock *pred : predecessors(current))
                    {
                        if (visitedBlocks.find(pred) == visitedBlocks.end() && blocks.find(pred) == blocks.end())
                        {
                            visitedBlocks.insert(pred);
                            workingSet.push(pred);
                        }
                    }
                }
                if (ent)
                {
                    result.insert(block);
                }
            }
        }
        return result;
    }

    set<BasicBlock *> GetVisitable(BasicBlock *base, set<BasicBlock *> &validBlocks)
    {
        queue<BasicBlock *> workingSet;
        set<BasicBlock *> visitedBlocks;
        workingSet.push(base);
        visitedBlocks.insert(base);
        while (!workingSet.empty())
        {
            BasicBlock *b = workingSet.front();
            cout << "process" << GetBlockID(b) << "\n";
            workingSet.pop();
            for (auto suc : successors(b))
            {
                int id = GetBlockID(suc);
                if (validBlocks.find(suc) != validBlocks.end() && visitedBlocks.find(suc) == visitedBlocks.end())
                {
                    cout << "push" << id << "\n";
                    workingSet.push(suc);
                    visitedBlocks.insert(suc);
                }
            }
            for (auto bi = b->begin(); bi != b->end(); bi++)
            {
                if (auto cb = dyn_cast<CallBase>(bi))
                {
                    Function *F = cb->getCalledFunction();
                    if (F && !F->empty())
                    {
                        BasicBlock *c = &F->getEntryBlock();
                        if (validBlocks.find(c) != validBlocks.end() && visitedBlocks.find(c) == visitedBlocks.end())
                        {
                            workingSet.push(c);
                            visitedBlocks.insert(c);
                        }
                    }
                }
            }
        }
        return visitedBlocks;
    }
} // namespace TypeFour