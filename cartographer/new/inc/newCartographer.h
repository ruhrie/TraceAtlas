#pragma once
#include <deque>
#include <map>
#include <set>

enum class NodeColor
{
    White,
    Grey,
    Black
};

struct DijkstraNode
{
    DijkstraNode() = default;
    DijkstraNode(double d, uint64_t p, NodeColor c)
    {
        distance = d;
        predecessor = p;
        color = c;
    }
    /// distance between this node and the target source node
    /// since our objective is to find the maximum likelihood path, we need to map probabilities onto a space that minimizes big probabilities and maximizes small ones
    /// -log(p) is how we do this
    double distance;
    /// minimum-distance predecessor of this node
    uint64_t predecessor;
    /// whether or not this node has been investigated
    NodeColor color;
};

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

struct Kernel
{
    Kernel()
    {
        KID = getNextKID();
        nodes = std::set<GraphNode, GNCompare>();
    }
    Kernel(uint32_t ID)
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
    /// @brief Compares this kernel to another kernel by measuring node differences
    ///
    /// If two kernels are the same, 1 will be returned
    /// If two kernels are completely different, 0 will be returned
    /// If two kernels share some nodes, (compare shared) / (this shared) will be returned
    float Compare(const Kernel &compare) const
    {
        int compShared = 0;
        for (const auto &compNode : compare.nodes)
        {
            if (nodes.find(compNode) != nodes.end())
            {
                compShared++;
            }
        }
        return (float)((float)compShared / (float)(nodes.size()));
    }
    /// Returns true is any node in the kernel can reach every other node in the kernel. False otherwise
    bool FullyConnected() const
    {
        for (const auto &node : nodes)
        {
            // keeps track of which nodeIDs have been visited, all initialized to white
            std::map<uint64_t, NodeColor> colors;
            for (const auto &node2 : nodes)
            {
                colors[node2.NID] = NodeColor::White;
            }
            // holds newly discovered nodes
            std::deque<GraphNode> Q;
            Q.push_back(node);
            while (!Q.empty())
            {
                for (const auto &neighbor : Q.front().neighbors)
                {
                    // check if this neighbor is within the kernel
                    if (nodes.find(neighbor.first) != nodes.end())
                    {
                        if (colors[neighbor.first] == NodeColor::White)
                        {
                            Q.push_back(*(nodes.find(neighbor.first)));
                        }
                    }
                    colors[neighbor.first] = NodeColor::Black;
                }
                Q.pop_front();
            }
            // if any nodes in the kernel have not been touched, this node couldn't reach them
            for (const auto &node : colors)
            {
                if (node.second == NodeColor::White)
                {
                    return false;
                }
            }
        }
        return true;
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
struct KCompare
{
    using is_transparent = void;
    bool operator()(const Kernel &lhs, const Kernel &rhs) const
    {
        return lhs.KID < rhs.KID;
    }
    bool operator()(const Kernel &lhs, uint64_t rhs) const
    {
        return lhs.KID < rhs;
    }
    bool operator()(uint64_t lhs, const Kernel &rhs) const
    {
        return lhs < rhs.KID;
    }
};