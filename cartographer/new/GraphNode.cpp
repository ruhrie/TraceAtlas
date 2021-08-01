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

bool GraphNode::mergeSuccessor(const GraphNode& succ)
{
    // the blocks of the node simply get added, if unique
    blocks.insert(succ.blocks.begin(), succ.blocks.end());
    // the original blocks have to be added in order such that we preserve which original block ID is the current block, and which blocks preceded it (in the order they executed)
    for( const auto& block : succ.originalBlocks )
    {
        bool found = false;
        for( const auto& b : originalBlocks )
        {
            if( block == b )
            {
                found = true;
            }
        }
        if( !found )
        {
            originalBlocks.push_back(block);
        }
    }
    return true;
}