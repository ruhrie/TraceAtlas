#pragma once
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <queue>
#include <set>

static std::set<llvm::BasicBlock *> GetReachable(llvm::BasicBlock *base, std::set<int64_t> validBlocks)
{
    bool foundSelf = false;
    std::queue<llvm::BasicBlock *> toProcess;
    std::set<llvm::BasicBlock *> checked;
    toProcess.push(base);
    checked.insert(base);
    while (!toProcess.empty())
    {
        auto bb = toProcess.front();
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
            if (auto ci = llvm::dyn_cast<llvm::CallBase>(bi))
            {
                auto f = ci->getCalledFunction();
                if (f != nullptr && !f->empty())
                {
                    auto entry = &f->getEntryBlock();
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

static bool IsReachable(llvm::BasicBlock *base, llvm::BasicBlock *target, const std::set<int64_t> &validBlocks)
{
    bool foundTarget = false;
    std::queue<llvm::BasicBlock *> toProcess;
    std::set<llvm::BasicBlock *> checked;
    toProcess.push(base);
    checked.insert(base);
    while (!toProcess.empty())
    {
        auto bb = toProcess.front();
        toProcess.pop();
        for (auto suc : successors(bb))
        {
            if (suc == target)
            {
                foundTarget = true;
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
            if (auto ci = llvm::dyn_cast<llvm::CallBase>(bi))
            {
                auto f = ci->getCalledFunction();
                if (f != nullptr && !f->empty())
                {
                    auto entry = &f->getEntryBlock();
                    if (entry == target)
                    {
                        foundTarget = true;
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
        llvm::Instruction *I = bb->getTerminator();
        if (auto ri = llvm::dyn_cast<llvm::ReturnInst>(I))
        {
            auto f = bb->getParent();
            for (auto use : f->users())
            {
                if (auto cb = llvm::dyn_cast<llvm::CallBase>(use))
                {
                    auto entry = cb->getParent();
                    if (entry == target)
                    {
                        foundTarget = true;
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
        else if (auto ri = llvm::dyn_cast<llvm::ResumeInst>(I))
        {
            auto f = bb->getParent();
            for (auto use : f->users())
            {
                if (auto cb = llvm::dyn_cast<llvm::CallBase>(use))
                {
                    auto entry = cb->getParent();
                    if (entry == target)
                    {
                        foundTarget = true;
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

    return foundTarget;
}

static bool IsSelfReachable(llvm::BasicBlock *base, const std::set<int64_t> &validBlocks)
{
    return IsReachable(base, base, validBlocks);
}

std::set<llvm::BasicBlock *> GetConditionals(const std::set<llvm::BasicBlock *> &blocks, const std::set<int64_t> &validBlocks)
{
    std::set<llvm::BasicBlock *> result;
    //a conditional is defined by a successor that cannot reach the conditional again
    for (auto block : blocks)
    {
        for (auto suc : successors(block))
        {
            if (!IsReachable(suc, block, validBlocks))
            {
                result.insert(block);
            }
            else if (validBlocks.find(GetBlockID(suc)) == validBlocks.end())
            {
                result.insert(block);
            }
        }
    }

    return result;
}

bool HasEpilogue(const std::set<llvm::BasicBlock *> &blocks, const std::set<int64_t> &validBlocks)
{
    auto conds = GetConditionals(blocks, validBlocks);
    for (auto cond : conds)
    {
        for (auto suc : successors(cond))
        {
            if (validBlocks.find(GetBlockID(suc)) != validBlocks.end())
            {
                if (!IsReachable(suc, cond, validBlocks))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

static std::set<llvm::BasicBlock *> GetExits(std::set<llvm::BasicBlock *> blocks)
{
    std::set<llvm::BasicBlock *> Exits;
    for (auto block : blocks)
    {
        for (auto suc : successors(block))
        {
            if (blocks.find(suc) == blocks.end())
            {
                Exits.insert(suc);
            }
        }
        auto term = block->getTerminator();
        if (auto retInst = llvm::dyn_cast<llvm::ReturnInst>(term))
        {
            auto base = block->getParent();
            for (auto user : base->users())
            {
                if (auto v = llvm::dyn_cast<llvm::Instruction>(user))
                {
                    auto parentBlock = v->getParent();
                    if (blocks.find(parentBlock) == blocks.end())
                    {
                        Exits.insert(parentBlock);
                    }
                }
            }
        }
    }
    return Exits;
}

static std::set<llvm::BasicBlock *> GetEntrances(std::set<llvm::BasicBlock *> &blocks)
{
    std::set<llvm::BasicBlock *> Entrances;
    for (auto block : blocks)
    {
        auto F = block->getParent();
        auto par = &F->getEntryBlock();
        //we first check if the block is an entry block, if it is the only way it could be an entry is through a function call
        if (block == par)
        {
            bool exte = false;
            bool inte = false;
            for (auto user : F->users())
            {
                if (auto cb = llvm::dyn_cast<llvm::CallBase>(user))
                {
                    auto parent = cb->getParent(); //the parent of the function call
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
            std::queue<llvm::BasicBlock *> workingSet;
            std::set<llvm::BasicBlock *> visitedBlocks;
            workingSet.push(block);
            visitedBlocks.insert(block);
            while (!workingSet.empty())
            {
                auto current = workingSet.back();
                workingSet.pop();
                if (current == par)
                {
                    //this is the entry block. We know that there is a valid path through the computation to here
                    //that doesn't somehow originate in the kernel
                    //this is guaranteed by the prior type 2 detector in cartographer
                    ent = true;
                    break;
                }
                for (llvm::BasicBlock *pred : predecessors(current))
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