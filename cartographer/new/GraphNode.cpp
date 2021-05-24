#include "GraphNode.h"

using namespace TraceAtlas::Cartographer;

uint64_t GraphNode::nextNID = 0;

GraphNode::GraphNode()
{
    NID = getNextNID();
    blocks = std::map<int64_t, int64_t>();
    neighbors = std::map<uint64_t, std::pair<uint64_t, double>>();
    predecessors = std::set<uint64_t>();
}

GraphNode::GraphNode(uint64_t ID)
{
    NID = ID;
    blocks = std::map<int64_t, int64_t>();
    neighbors = std::map<uint64_t, std::pair<uint64_t, double>>();
    predecessors = std::set<uint64_t>();
}

GraphNode::~GraphNode() = default;

void GraphNode::addBlock(int64_t newBlock)
{
    // to add a block
    // 1.) find the key that maps to itself
    // 2.) update that value to addBlock
    // 3.) add a new pair to the map (newBlock,newBlock)
    for (auto &k : blocks)
    {
        if (k.first == k.second)
        {
            k.second = newBlock;
            break;
        }
    }
    blocks[newBlock] = newBlock;
}
void GraphNode::addBlocks(const std::map<int64_t, int64_t> &newBlocks)
{
    // TODO: finish this
    for (const auto &k : newBlocks)
    {
        blocks[k.first] = k.first;
    }
    // to add this tree of blocks
    // 1.) find the block that ends the current node
    // 2.) connect that block to the end block
    // 3.) append the rest of the new blocks to the map
}

uint64_t GraphNode::getNextNID()
{
    return nextNID++;
}