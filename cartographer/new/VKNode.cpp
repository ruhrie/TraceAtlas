#include "VKNode.h"

using namespace TraceAtlas::Cartographer;

VKNode::VKNode(struct Kernel *p_k)
{
    kernel = p_k;
}

VKNode::VKNode(const GraphNode &GN, struct Kernel *p_k)
{
    NID = GN.NID;
    kernel = p_k;
    blocks = GN.blocks;
    neighbors = GN.neighbors;
    predecessors = GN.predecessors;
}

void VKNode::addNodeBlocks(GraphNode *newNode)
{
    for (const auto &block : newNode->blocks)
    {
        blocks[block.first] = block.second;
    }
}