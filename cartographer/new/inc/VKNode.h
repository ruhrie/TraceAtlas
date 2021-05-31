#pragma once
#include "Kernel.h"
namespace TraceAtlas::Cartographer
{
    /// @brief Virtual Kernel Node
    ///
    /// Represents a virtualized kernel. A virtualized kernel node is a node that represents an entire kernel, and all nodes that once were within the subgraph itself are now contained within this structure
    class VKNode : public GraphNode
    {
    public:
        struct Kernel *kernel;
        VKNode(struct Kernel *p_k);
        VKNode(const GraphNode &GN, struct Kernel *p_k);
        ~VKNode() = default;
        void addNodeBlocks(GraphNode *newNode);
    };
} // namespace TraceAtlas::Cartographer