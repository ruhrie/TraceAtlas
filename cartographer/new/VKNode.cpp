#include "VKNode.h"

using namespace TraceAtlas::Cartographer;

VKNode::VKNode(struct Kernel *p_k)
{
    kernel = p_k;
    for (const auto &node : kernel->nodes)
    {
        blocks.insert(node.blocks.begin(), node.blocks.end());
    }
}

VKNode::VKNode(const GraphNode &GN, struct Kernel *p_k)
{
    NID = getNextNID();
    kernel = p_k;
    for (const auto &node : kernel->nodes)
    {
        blocks.insert(node.blocks.begin(), node.blocks.end());
    }
    neighbors = GN.neighbors;
    predecessors = GN.predecessors;
}