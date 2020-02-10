#include "tik/Dijkstra.h"
#include "AtlasUtil/Print.h"
#include "tik/Exceptions.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <queue>
using namespace std;
using namespace llvm;

map<BasicBlock *, int> SolveDijkstraBack(set<BasicBlock *> exits, set<BasicBlock *> blocks)
{
    map<BasicBlock *, int> blockDistances;
    for (auto block : blocks)
    {
        blockDistances[block] = INT32_MAX;
    }
    queue<BasicBlock *> dijQueue;
    for (auto exit : exits)
    {
        blockDistances[exit] = 0;
        dijQueue.push(exit);
    }

    set<BasicBlock *> visitedBlocks;
    while (dijQueue.size() != 0)
    {
        BasicBlock *currentBlock = dijQueue.front();
        dijQueue.pop();
        if (visitedBlocks.find(currentBlock) != visitedBlocks.end())
        {
            continue;
        }
        int currentDistance = blockDistances[currentBlock];
        int predCount = 0;
        for (auto pred : predecessors(currentBlock))
        {
            if (blocks.find(pred) != blocks.end() || exits.find(pred) != exits.end())
            {
                blockDistances[pred] = min(currentDistance + 1, blockDistances[pred]);
                dijQueue.push(pred);

                for (auto bi = pred->begin(); bi != pred->end(); bi++)
                {
                    if (auto ci = dyn_cast<CallInst>(bi))
                    {
                        Function *f = ci->getCalledFunction();
                        if (!f->empty())
                        {
                            vector<BasicBlock *> exitBlocks;
                            for (auto fi = f->begin(); fi != f->end(); fi++)
                            {
                                auto term = fi->getTerminator();
                                if (isa<ReturnInst>(term))
                                {
                                    exitBlocks.push_back(term->getParent());
                                }
                            }

                            for (auto exit : exitBlocks)
                            {
                                blockDistances[exit] = min(currentDistance + 1, blockDistances[exit]);
                                dijQueue.push(exit);
                            }
                            blockDistances[pred] = INT32_MAX;
                        }
                    }
                }
            }
            predCount++;
        }
        if (predCount == 0)
        {
            //I assume that this must be an entry block, but let's double check

            Function *f = currentBlock->getParent();
            if (currentBlock != &f->getEntryBlock())
            {
                throw 2;
            }
            for (auto user : f->users())
            {
                if (auto ci = dyn_cast<CallInst>(user))
                {
                    BasicBlock *parent = ci->getParent();
                    blockDistances[parent] = min(currentDistance + 1, blockDistances[parent]);
                }
                else if (auto ci = dyn_cast<InvokeInst>(user))
                {
                    BasicBlock *parent = ci->getParent();
                    blockDistances[parent] = min(currentDistance + 1, blockDistances[parent]);
                }
            }
        }
        visitedBlocks.insert(currentBlock);
    }
    return blockDistances;
}

map<BasicBlock *, int> SolveDijkstraFront(set<BasicBlock *> entrances, set<BasicBlock *> blocks)
{
    map<BasicBlock *, int> blockDistances;
    for (auto block : blocks)
    {
        blockDistances[block] = INT32_MAX;
    }
    queue<BasicBlock *> dijQueue;
    for (auto ent : entrances)
    {
        blockDistances[ent] = 0;
        dijQueue.push(ent);
    }

    set<BasicBlock *> visitedBlocks;
    while (dijQueue.size() != 0)
    {
        BasicBlock *currentBlock = dijQueue.front();
        dijQueue.pop();
        if (visitedBlocks.find(currentBlock) != visitedBlocks.end())
        {
            continue;
        }
        int currentDistance = blockDistances[currentBlock];
        int sucCount = 0;
        auto term = currentBlock->getTerminator();
        for (int i = 0; i < term->getNumSuccessors(); i++) //we loop through the successors
        {
            auto suc = term->getSuccessor(i);
            if (blocks.find(suc) != blocks.end() || entrances.find(suc) != entrances.end()) //if it is a valid block
            {
                //we give is an increment value
                blockDistances[suc] = min(currentDistance + 1, blockDistances[suc]);
                dijQueue.push(suc); //and mark it for later processing of its successors
            }
            sucCount++;
        }
        if (sucCount == 0)
        {
            //I assume that this must be a return block, but let's double check
            throw TikException("Unimplemented");
            Function *f = currentBlock->getParent();
            if (currentBlock != &f->getEntryBlock())
            {
                throw 2;
            }
        }
        for (auto bi = currentBlock->begin(); bi != currentBlock->end(); bi++)
        {
            if (auto ci = dyn_cast<CallInst>(bi))
            {
                Function *f = ci->getCalledFunction();
                if (!f->empty())
                {
                    auto entry = &f->getEntryBlock();
                    blockDistances[entry] = min(currentDistance + 1, blockDistances[entry]);
                    dijQueue.push(entry);
                }
            }
        }
        visitedBlocks.insert(currentBlock);
    }
    return blockDistances;
}

map<BasicBlock *, int> DijkstraII(set<BasicBlock *> entrances, set<BasicBlock *> blocks)
{
    map<BasicBlock *, int> blockDistances;
    for (auto block : blocks)
    {
        blockDistances[block] = INT32_MAX;
    }
    queue<BasicBlock *> dijQueue;
    for (auto ent : entrances)
    {
        blockDistances[ent] = 0;
        dijQueue.push(ent);
    }

    set<BasicBlock *> visitedBlocks;
    while (dijQueue.size() != 0)
    {
        BasicBlock *currentBlock = dijQueue.front();
        int currentDistance = blockDistances[currentBlock];
        dijQueue.pop();
        visitedBlocks.insert(currentBlock);
        for (auto suc : successors(currentBlock))
        {
            if (visitedBlocks.find(suc) == visitedBlocks.end())
            {
                dijQueue.push(suc);
            }
        }
        bool done = false;
        auto term = currentBlock->getTerminator();
        if (term != cast<Instruction>(currentBlock->begin()))
        {
            //it is more than one instruction long
            Instruction *penultimate = term->getPrevNode();
            if (auto ci = dyn_cast<CallBase>(penultimate))
            {
                Function *f = ci->getCalledFunction();
                if (!f->empty())
                {
                    auto entry = &f->getEntryBlock();
                    blockDistances[entry] = min(blockDistances[entry], currentDistance + 1);
                    dijQueue.push(entry);
                    done = true;
                }
            }
        }

        if (!done)
        {
            for (auto suc : successors(currentBlock))
            {
                blockDistances[suc] = min(blockDistances[suc], currentDistance + 1);
            }
        }

        if (isa<ReturnInst>(term))
        {
            Function *f = currentBlock->getParent();
            for (auto user : f->users())
            {
                if (auto fu = dyn_cast<Instruction>(user))
                {
                    BasicBlock *suc = fu->getParent()->getSingleSuccessor();
                    blockDistances[suc] = min(blockDistances[suc], currentDistance + 1);
                }
            }
        }
    }
    return blockDistances;
}

map<BasicBlock *, int> DijkstraIII(set<BasicBlock *> exits, set<BasicBlock *> blocks)
{
    map<BasicBlock *, int> blockDistances;
    for (auto block : blocks)
    {
        blockDistances[block] = INT32_MAX;
    }
    queue<BasicBlock *> dijQueue;
    for (auto ent : exits)
    {
        blockDistances[ent] = 0;
        dijQueue.push(ent);
    }

    set<BasicBlock *> visitedBlocks;
    while (dijQueue.size() != 0)
    {
        BasicBlock *currentBlock = dijQueue.front();
        visitedBlocks.insert(currentBlock);
        dijQueue.pop();
        if (blocks.find(currentBlock) == blocks.end() && exits.find(currentBlock) == exits.end())
        {
            continue;
        }
        int currentDistance = blockDistances[currentBlock];
        for (auto pred : predecessors(currentBlock))
        {
            blockDistances[pred] = min(blockDistances[pred], currentDistance + 1);
            if (visitedBlocks.find(pred) == visitedBlocks.end())
            {
                dijQueue.push(pred);
            }
        }
    }
    return blockDistances;
}