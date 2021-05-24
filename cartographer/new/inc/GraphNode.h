#pragma once
#include <cstdint>
#include <map>
#include <set>

namespace TraceAtlas::Cartographer
{
    class GraphNode
    {
    public:
        uint64_t NID;
        /// BBIDs from the source bitcode that are represented by this node
        /// Each key is a member BBID and its value is the basic block its unconditional edge points to
        /// If a key maps to itself, there is no edge attached to this block
        std::map<int64_t, int64_t> blocks;
        /// Maps a neighbor nodeID to a probability edge. The set of keys is comprehensive for all neighbors of this GraphNode
        /// The first index in the pair is the raw count, the second is the histogram probability
        std::map<uint64_t, std::pair<uint64_t, double>> neighbors;
        /// Holds the node IDs of each predecessor of this node
        std::set<uint64_t> predecessors;
        GraphNode();
        GraphNode(uint64_t);
        /// Meant to be constructed from a new block description in the input binary file
        virtual ~GraphNode();
        void addBlock(int64_t newBlock);
        void addBlocks(const std::map<int64_t, int64_t> &newBlocks);

    protected:
        static uint64_t nextNID;
        static uint64_t getNextNID();
    };

    /// Allows for us to search a set of GraphNodes using an NID
    struct p_GNCompare
    {
        using is_transparent = void;
        bool operator()(const GraphNode *lhs, const GraphNode *rhs) const
        {
            return lhs->NID < rhs->NID;
        }
        bool operator()(const GraphNode *lhs, uint64_t rhs) const
        {
            return lhs->NID < rhs;
        }
        bool operator()(uint64_t lhs, const GraphNode *rhs) const
        {
            return lhs < rhs->NID;
        }
    };

    struct GNCompare
    {
        using is_transparent = void;
        bool operator()(const GraphNode &lhs, const GraphNode &rhs) const
        {
            return lhs.NID < rhs.NID;
        }
        bool operator()(const GraphNode &lhs, uint64_t rhs) const
        {
            return lhs.NID < rhs;
        }
        bool operator()(uint64_t lhs, const GraphNode &rhs) const
        {
            return lhs < rhs.NID;
        }
    };
} // namespace TraceAtlas::Cartographer