#pragma once
#include "GraphNode.h"
#include <deque>
#include <string>
#include <vector>

namespace TraceAtlas::Cartographer
{
    class VKNode;
    struct Kernel
    {
        std::set<GraphNode, GNCompare> nodes;
        /// set of KIDs that point to child kernels of this kernel
        std::set<uint32_t> childKernels;
        std::set<uint32_t> parentKernels;
        VKNode *virtualNode;
        uint32_t KID;
        std::string Label;
        Kernel();
        Kernel(uint32_t ID);
        /// Returns copies of the kernel entrances nodes. These nodes were copied when the kernel was generated, and any changes to them will not be present in the control flow graph
        /// @retval    entrances Vector of IDs that specify which nodes (or blocks) are the kernel entrances. Kernel entrances are the source nodes of edges that enter the kernel
        std::vector<GraphNode> getEntrances() const;
        /// Returns pointers to the kernel entrance nodes of the kernel within the CFG argument. All changes made to the entrances will be present in the CFG.
        std::vector<GraphNode *> getEntrances(std::set<GraphNode *, p_GNCompare> &CFG) const;
        /// @brief Returns the IDs of the kernel exits
        ///
        /// @param[in] allNodes Set of all nodes in the control flow graph. Used to copy the nodes that are the destinations of edges that leave the kernel
        /// @retval    exits Vector of IDs that specify which nodes (or blocks) are the kernel exits. Kernel exits are nodes that have a neighbor outside the kernel
        std::vector<GraphNode> getExits() const;
        /// Returns pointers to the kernel entrance nodes of the kernel within the CFG argument. All changes made to the entrances will be present in the CFG.
        std::vector<GraphNode *> getExits(std::set<GraphNode *, p_GNCompare> &CFG) const;
        /// @brief Returns the member blocks (from the source bitcode) of this kernel
        std::set<int64_t> getBlocks() const;
        /// @brief Compares this kernel to another kernel by measuring node differences
        ///
        /// If two kernels are the same, 1 will be returned
        /// If two kernels are completely different, 0 will be returned
        /// If two kernels share some nodes, (compare shared) / (this size) will be returned
        /// TODO: if this object fully overlaps with compare, but compare contains other blocks, this will say that we fully match when we actually don't. Fix that
        std::set<int64_t> Compare(const Kernel &compare) const;
        /// Returns true if any node in the kernel can reach every other node in the kernel. False otherwise
        bool FullyConnected() const;
        /// Returns the probability that this kernel keeps recurring vs. exiting
        float ExitProbability() const;
        inline bool operator==(const Kernel &rhs) const;

    private:
        static uint32_t nextKID;
        static uint32_t getNextKID();
    };

    /// Allows for us to search a set of GraphNodes using an NID
    struct KCompare
    {
        using is_transparent = void;
        bool operator()(const Kernel *lhs, const Kernel *rhs) const
        {
            return lhs->KID < rhs->KID;
        }
        bool operator()(const Kernel *lhs, uint64_t rhs) const
        {
            return lhs->KID < rhs;
        }
        bool operator()(uint64_t lhs, const Kernel *rhs) const
        {
            return lhs < rhs->KID;
        }
    };
} // namespace TraceAtlas::Cartographer