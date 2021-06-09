#include "GraphNode.h"

using namespace TraceAtlas::Cartographer;

uint64_t GraphNode::nextNID = 0;

GraphNode::GraphNode()
{
    NID = getNextNID();
    blocks = std::set<int64_t>();
    neighbors = std::map<uint64_t, std::pair<uint64_t, double>>();
    predecessors = std::set<uint64_t>();
}

GraphNode::GraphNode(uint64_t ID)
{
    NID = ID;
    // keep next ID updated
    nextNID = NID > nextNID ? NID : nextNID;
    blocks = std::set<int64_t>();
    neighbors = std::map<uint64_t, std::pair<uint64_t, double>>();
    predecessors = std::set<uint64_t>();
}

GraphNode::~GraphNode() = default;

void GraphNode::addBlock(int64_t newBlock)
{
    blocks.insert(newBlock);
}

void GraphNode::addBlocks(const std::set<int64_t> &newBlocks)
{
    blocks.insert(newBlocks.begin(), newBlocks.end());
}

uint64_t GraphNode::getNextNID()
{
    return nextNID++;
}