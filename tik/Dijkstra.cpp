#include "tik/Dijkstra.h"
#include "tik/Exceptions.h"
#include "AtlasUtil/Print.h"
#include <llvm/IR/CFG.h>
#include <queue>
using namespace std;
using namespace llvm;

map<BasicBlock *, int> SolveDijkstraBack(set<BasicBlock *> exits, set<BasicBlock *> blocks)
{
    map<BasicBlock *, int> blockDistances;
    for(auto block : blocks)
    {
        blockDistances[block] = INT32_MAX;
    }
    queue<BasicBlock *> dijQueue;
    for(auto exit : exits)
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
            }
            predCount++;
        }
        if (predCount == 0)
        {
            //I assume that this must be an entry block, but let's double check
            throw TikException("Unimplemented");
            Function *f = currentBlock->getParent();
            if (currentBlock != &f->getEntryBlock())
            {
                throw 2;
            }
        }
        visitedBlocks.insert(currentBlock);
    }
    return blockDistances;
}

map<BasicBlock *, int> SolveDijkstraFront(set<BasicBlock *> entrances, set<BasicBlock *> blocks)
{
    map<BasicBlock *, int> blockDistances;
    for(auto block : blocks)
    {
        blockDistances[block] = INT32_MAX;
    }
    queue<BasicBlock *> dijQueue;
    for(auto ent : entrances)
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
        for(int i = 0; i < term->getNumSuccessors(); i++)
        {
            auto suc = term->getSuccessor(i);
            if (blocks.find(suc) != blocks.end() || entrances.find(suc) != entrances.end())
            {
                blockDistances[suc] = min(currentDistance + 1, blockDistances[suc]);
                dijQueue.push(suc);
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
        visitedBlocks.insert(currentBlock);
    }
    return blockDistances;
}