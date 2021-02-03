#pragma once
#include <map>
#include <set>

struct GraphNode
{
    uint64_t NID;
    /// BBIDs from the source bitcode that are represented by this node
    std::set<uint64_t> blocks;
    /// Maps a neighbor nodeID to a probability edge. The set of keys is comprehensive for all neighbors of this GraphNode
    /// The first index in the pair is the raw count, the second is the histogram probability
    std::map<uint64_t, std::pair<uint64_t, double>> neighbors;
    GraphNode()
    {
        NID = getNextNID();
        blocks = std::set<uint64_t>();
        neighbors = std::map<uint64_t, std::pair<uint64_t, double>>();
    }
    /// @brief BBID constructor
    ///
    /// Meant to be constructed from a new block description in the input binary file
    GraphNode(uint64_t ID)
    {
        NID = ID;
        blocks = std::set<uint64_t>();
        neighbors = std::map<uint64_t, std::pair<uint64_t, double>>();
    }
    static uint64_t nextNID;
    static uint64_t getNextNID()
    {
        return nextNID++;
    }
};

/// Allows for us to search a set of GraphNodes using an NID
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

struct MinKernel
{
    MinKernel()
    {
        KID = getNextKID();
        nodes = std::set<GraphNode, GNCompare>();
    }
    MinKernel(uint32_t ID)
    {
        KID = ID;
        nodes = std::set<GraphNode, GNCompare>();
    }
    const std::set<uint64_t> getBlocks() const
    {
        std::set<uint64_t> blocks;
        for (const auto &node : nodes)
        {
            blocks.insert(node.blocks.begin(), node.blocks.end());
        }
        return blocks;
    }
    std::set<GraphNode, GNCompare> nodes;
    uint32_t KID;
    static uint32_t nextKID;
    static uint32_t getNextKID()
    {
        return nextKID++;
    }
};

/// Allows for us to search a set of GraphNodes using an NID
struct MKCompare
{
    using is_transparent = void;
    bool operator()(const MinKernel &lhs, const MinKernel &rhs) const
    {
        return lhs.KID < rhs.KID;
    }
    bool operator()(const MinKernel &lhs, uint64_t rhs) const
    {
        return lhs.KID < rhs;
    }
    bool operator()(uint64_t lhs, const MinKernel &rhs) const
    {
        return lhs < rhs.KID;
    }
};