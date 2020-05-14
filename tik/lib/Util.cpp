#include "tik/Util.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#include <queue>
#include <regex>

using namespace std;
using namespace llvm;

namespace TraceAtlas::tik
{
    string GetString(Value *v)
    {
        std::string str;
        llvm::raw_string_ostream rso(str);
        v->print(rso);
        str = std::regex_replace(str, std::regex("^\\s+"), std::string(""));
        str = std::regex_replace(str, std::regex("\\s+$"), std::string(""));
        return str;
    }

    std::vector<std::string> GetStrings(BasicBlock *bb)
    {
        std::vector<std::string> result;
        for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
        {
            auto *inst = cast<Instruction>(BI);
            result.push_back(GetString(inst));
        }
        return result;
    }

    std::map<std::string, vector<string>> GetStrings(Function *f)
    {
        map<string, vector<string>> result;
        result["Body"] = GetStrings(&f->getEntryBlock());
        vector<string> args;
        for (Argument *i = f->arg_begin(); i < f->arg_end(); i++)
        {
            args.push_back(GetString(i));
        }
        if (!args.empty())
        {
            result["Inputs"] = args;
        }
        return result;
    }

    std::vector<std::string> GetStrings(const std::set<Instruction *> &instructions)
    {
        std::vector<std::string> result(instructions.size());
        for (Instruction *inst : instructions)
        {
            result.push_back(GetString(inst));
        }
        return result;
    }

    std::vector<std::string> GetStrings(const std::vector<Instruction *> &instructions)
    {
        std::vector<std::string> result;
        for (Instruction *inst : instructions)
        {
            std::string str;
            llvm::raw_string_ostream rso(str);
            inst->print(rso);
            result.push_back(GetString(inst));
        }
        return result;
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
        }

        if (!foundSelf)
        {
            checked.erase(base);
        }
        return checked;
    }
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

    bool IsReachable(BasicBlock *base, BasicBlock *target, const set<int64_t> &validBlocks)
    {
        bool foundTarget = false;
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
                if (auto ci = dyn_cast<CallBase>(bi))
                {
                    Function *f = ci->getCalledFunction();
                    if (f != nullptr && !f->empty())
                    {
                        BasicBlock *entry = &f->getEntryBlock();
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
            Instruction *I = bb->getTerminator();
            if (auto ri = dyn_cast<ReturnInst>(I))
            {
                Function *f = bb->getParent();
                for (auto use : f->users())
                {
                    if (auto cb = dyn_cast<CallBase>(use))
                    {
                        BasicBlock *entry = cb->getParent();
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
            else if (auto ri = dyn_cast<ResumeInst>(I))
            {
                Function *f = bb->getParent();
                for (auto use : f->users())
                {
                    if (auto cb = dyn_cast<CallBase>(use))
                    {
                        BasicBlock *entry = cb->getParent();
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

    bool IsSelfReachable(BasicBlock *base, const set<int64_t> &validBlocks)
    {
        return IsReachable(base, base, validBlocks);
    }

    set<BasicBlock *> GetExits(set<BasicBlock *> blocks)
    {
        set<BasicBlock *> Exits;
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
            if (auto retInst = dyn_cast<ReturnInst>(term))
            {
                Function *base = block->getParent();
                for (auto user : base->users())
                {
                    if (auto v = dyn_cast<Instruction>(user))
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
    set<BasicBlock *> GetConditionals(const set<BasicBlock *> &blocks, const set<int64_t> &validBlocks)
    {
        set<BasicBlock *> result;
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

    bool HasEpilogue(const set<BasicBlock *> &blocks, const set<int64_t> &validBlocks)
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

    set<BasicBlock *> GetExits(Function *F)
    {
        set<BasicBlock *> result;

        for (auto fi = F->begin(); fi != F->end(); fi++)
        {
            auto block = cast<BasicBlock>(fi);
            for (auto suc : successors(block))
            {
                if (suc->getParent() != F)
                {
                    result.insert(suc);
                }
            }
        }
        return result;
    }
} // namespace TraceAtlas::tik
