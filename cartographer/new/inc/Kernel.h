#pragma once
#include "VKNode.h"
#include <deque>
#include <string>
#include <vector>

namespace TraceAtlas::Cartographer
{
    struct Kernel
    {
        std::set<GraphNode, GNCompare> nodes;
        VKNode *kernelNode;
        uint32_t KID;
        std::string Label;
        Kernel();
        Kernel(uint32_t ID);
        /// Returns the IDs of the kernel entrances
        /// @retval    entrances Vector of IDs that specify which nodes (or blocks) are the kernel entrances. Kernel entrances are the source nodes of edges that enter the kernel
        std::vector<GraphNode> getEntrances() const;
        /// @brief Returns the IDs of the kernel exits
        ///
        /// @param[in] allNodes Set of all nodes in the control flow graph. Used to copy the nodes that are the destinations of edges that leave the kernel
        /// @retval    exits Vector of IDs that specify which nodes (or blocks) are the kernel exits. Kernel exits are nodes that have a neighbor outside the kernel
        std::vector<GraphNode> getExits() const;
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