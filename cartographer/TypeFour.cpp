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
    ///Represents a basic block and a function call signiture, only for local use
    struct BlockSigniture
    {
        BasicBlock *Block;
        int64_t FunctionID;
        BlockSigniture(BasicBlock *block) //default the functionid to -1
        {
            Block = block;
            FunctionID = -1;
        }
        BlockSigniture(BasicBlock *block, int64_t function)
        {
            Block = block;
            FunctionID = function;
        }
        bool operator==(const BlockSigniture &lhs) const //needed for searching
        {
            return Block == lhs.Block && FunctionID == lhs.FunctionID;
        }
        bool operator<(const BlockSigniture &lhs) const
        {
            bool result;
            if (Block < lhs.Block)
            {
                result = true;
            }
            else if (Block > lhs.Block)
            {
                result = false;
            }
            else
            {
                result = FunctionID < lhs.FunctionID;
            }
            return result;
        }
    };

    map<CallBase *, int64_t> functionIdMap; //dictionary for function id lookup, NOT for direct use!
    int64_t nextId = 0;                     //next id for the function map, NOT for direct use!
    int64_t getFunctionId(CallBase *b)      //if you need a new id, use this function, it will lookup the id or assign a new one and return that
    {
        if (functionIdMap.find(b) == functionIdMap.end())
        {
            functionIdMap[b] = nextId++;
        }
        return functionIdMap[b];
    }

    void GetReachable(const BlockSigniture &base, vector<BlockSigniture> &result, const set<int64_t> &validBlocks)
    {
        //so we will start by adding every successor, they are in the same function so they have the same function id
        for (auto suc : successors(base.Block))
        {
            int64_t id = GetBlockID(suc);
            if (validBlocks.find(id) != validBlocks.end())
            {
                auto candidate = BlockSigniture(suc, base.FunctionID);
                if (find(result.begin(), result.end(), candidate) == result.end())
                {
                    result.push_back(candidate);
                    GetReachable(candidate, result, validBlocks);
                }
            }
        }
        //next we need to check every intstruction
        for (auto bi = base.Block->begin(); bi != base.Block->end(); bi++)
        {
            //if it is a call, we will get a block id and add it to the queue
            if (auto cb = dyn_cast<CallBase>(bi))
            {
                Function *F = cb->getCalledFunction();
                if (F != nullptr && !F->empty())
                {
                    BasicBlock *entry = &F->getEntryBlock();
                    auto entryId = GetBlockID(entry);
                    if (validBlocks.find(entryId) != validBlocks.end())
                    {
                        auto candidate = BlockSigniture(entry, getFunctionId(cb));
                        if (find(result.begin(), result.end(), candidate) == result.end())
                        {
                            result.push_back(candidate);
                            GetReachable(candidate, result, validBlocks);
                        }
                    }
                }
            }
            //if it is a return (or exception) we go into the virtual caller
            else if (isa<ReturnInst>(bi) || isa<ResumeInst>(bi))
            {
                if (base.FunctionID == -1) //we only need to care if it doesn't have a function id, otherwise it is already handeled
                {
                    //we are in a function at the start
                    //get the number of uses (call base)
                    //duplicate every entry in result where the function id is -1 with the new id, these are the block in the current function that haven't been mapped
                    //then push back into the result each calling block and remove the prior not incorrect blocks
                    //this is order dependent, unlike the rest of the code
                    vector<CallBase *> calls;
                    Function *F = base.Block->getParent();
                    for (auto user : F->users())
                    {
                        if (auto *cb = dyn_cast<CallBase>(user))
                        {
                            auto parId = GetBlockID(cb->getParent());
                            if (validBlocks.find(parId) != validBlocks.end())
                            {
                                calls.push_back(cb);
                            }
                        }
                    }
                    vector<BlockSigniture> toRemove;
                    for (auto r : result)
                    {
                        if (r.FunctionID == -1)
                        {
                            for (auto call : calls)
                            {
                                if (call->getFunction() == r.Block->getParent())
                                {
                                    auto fId = getFunctionId(call);
                                    auto newR = BlockSigniture(r.Block, fId);
                                    result.push_back(newR);
                                }
                            }
                            toRemove.push_back(r);
                        }
                    }
                    for (auto r : toRemove)
                    {
                        result.erase(remove(result.begin(), result.end(), r), result.end());
                    }
                    //now that we repaired the result retroactively, we can recurse once for each caller
                    //we don't know if this needs a function id, so we just pass -1 for the time being
                    //worst case this is going to chain and operate again above to repair
                    for (auto call : calls)
                    {
                        auto parent = call->getParent();
                        auto candidate = BlockSigniture(parent);
                        //we will check for the id here, but I'd be shocked if it was already added
                        //better to be safe though
                        if (find(result.begin(), result.end(), candidate) == result.end())
                        {
                            result.push_back(candidate);
                            GetReachable(candidate, result, validBlocks);
                        }
                    }
                }
            }
        }
    }

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
                vector<BlockSigniture> sigReachable;
                GetReachable(BlockSigniture(base), sigReachable, kernel);
                for (auto reach : sigReachable)
                {
                    if (reach.Block == base)
                    {
                        blocks.insert(block);
                        break;
                    }
                }
            }
            //blocks is now a set, but it may be disjoint, so we need to check that now
            /*
            map<int64_t, set<BasicBlock *>> reachableBlockSets;
            for (auto block : blocks)
            {
                auto base = blockMap[block];
                reachableBlockSets[block] = GetReachable(base, blocks);
            }
            set<set<int64_t>> subSets;
            for (auto block : blocks)
            {
                set<int64_t> sub;
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
            */

            set<BasicBlock *> blockSet;
            for (auto block : blocks)
            {
                blockSet.insert(blockMap[block]);
            }

            set<BasicBlock *> entrances = GetEntrances(blockSet);

            for (auto ent : entrances)
            {
                set<BlockSigniture> sigSet;
                vector<BlockSigniture> a;
                GetReachable(BlockSigniture(ent), a, blocks);
                set<int64_t> b;
                for (auto as : a)
                {
                    b.insert(GetBlockID(as.Block));
                }
                result.insert(b);
            }

            /*
            for (const auto &subSet : subSets)
            {
                result.insert(subSet);
            }*/
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