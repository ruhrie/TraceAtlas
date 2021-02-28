#pragma once
#include <deque>
#include <map>
#include <set>
#include <vector>

enum class NodeColor
{
    White,
    Grey,
    Black
};

template <typename T>
struct PairComp
{
    using is_transparent = void;
    bool operator()(const std::pair<T, T> &lhs, const std::pair<T, T> &rhs) const
    {
        return lhs.first < rhs.first;
    }
    bool operator()(const std::pair<T, T> &lhs, T rhs) const
    {
        return lhs.first < rhs;
    }
    bool operator()(T lhs, const std::pair<T, T> &rhs) const
    {
        return lhs < rhs.first;
    }
};

struct DijkstraNode
{
    DijkstraNode() = default;
    DijkstraNode(double d, uint64_t id, uint64_t p, NodeColor c)
    {
        distance = d;
        NID = id;
        predecessor = p;
        color = c;
    }
    uint64_t NID; // maps this dijkstra node to a GraphNode.NID
    /// distance between this node and the target source node
    /// since our objective is to find the maximum likelihood path, we need to map probabilities onto a space that minimizes big probabilities and maximizes small ones
    /// -log(p) is how we do this
    double distance;
    /// minimum-distance predecessor of this node
    uint64_t predecessor;
    /// whether or not this node has been investigated
    NodeColor color;
};

struct DijkstraCompare
{
    using is_transparent = void;
    bool operator()(const DijkstraNode &lhs, const DijkstraNode &rhs) const
    {
        return lhs.distance < rhs.distance;
    }
    /*bool operator()(const DijkstraNode &lhs, uint64_t rhs) const
    {
        return lhs.distance < rhs;
    }
    bool operator()(uint64_t lhs, const DijkstraNode &rhs) const
    {
        return lhs < rhs.distance;
    }*/
} DCompare;
struct GraphNode
{
    uint64_t NID;
    /// BBIDs from the source bitcode that are represented by this node
    /// Each key is a member BBID and its value is the basic block its unconditional edge points to
    /// If a key maps to itself, there is no edge attached to this block
    std::map<uint64_t, uint64_t> blocks;
    /// Maps a neighbor nodeID to a probability edge. The set of keys is comprehensive for all neighbors of this GraphNode
    /// The first index in the pair is the raw count, the second is the histogram probability
    std::map<uint64_t, std::pair<uint64_t, double>> neighbors;
    /// Holds the node IDs of each predecessor of this node
    std::set<uint64_t> predecessors;
    GraphNode()
    {
        NID = getNextNID();
        blocks = std::map<uint64_t, uint64_t>();
        neighbors = std::map<uint64_t, std::pair<uint64_t, double>>();
        predecessors = std::set<uint64_t>();
    }
    /// Meant to be constructed from a new block description in the input binary file
    GraphNode(uint64_t ID)
    {
        NID = ID;
        blocks = std::map<uint64_t, uint64_t>();
        neighbors = std::map<uint64_t, std::pair<uint64_t, double>>();
        predecessors = std::set<uint64_t>();
    }
    void addBlock(uint64_t newBlock)
    {
        // to add a block
        // 1.) find the key that maps to itself
        // 2.) update that value to addBlock
        // 3.) add a new pair to the map (newBlock,newBlock)
        for (auto &k : blocks)
        {
            if (k.first == k.second)
            {
                k.second = newBlock;
                break;
            }
        }
        blocks[newBlock] = newBlock;
    }
    void addBlocks(const std::map<uint64_t, uint64_t> &newBlocks)
    {
        // TODO: finish this
        for (const auto &k : newBlocks)
        {
            blocks[k.first] = k.first;
        }
        // to add this tree of blocks
        // 1.) find the block that ends the current node
        // 2.) connect that block to the end block
        // 3.) append the rest of the new blocks to the map
    }

private:
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

/// @brief Dijkstra implementation inspired by boost library
std::vector<uint64_t> Dijkstras(const std::set<GraphNode, GNCompare> &nodes, uint64_t source, uint64_t sink);

struct Kernel
{
    std::set<GraphNode, GNCompare> nodes;
    uint32_t KID;
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
    /// @brief Returns the IDs of the kernel entrances
    ///
    /// @param[in] block     When true, this method will return the ID of the block from the original source code. When false, it returns the GraphNode ID
    /// @retval    entrances Vector of IDs that specify which nodes (or blocks) are the kernel entrances. Kernel entrances are the source nodes of edges that enter the kernel
    /*const std::vector<uint64_t> getEntrances(bool block = false) const
    {
        for( const auto& node : nodes )
        {
            // need to find out if we can reach this node
        }
    }*/
    /// @brief Returns the IDs of the kernel exits
    ///
    /// @param[in] block When true, this method will return the ID of the block from the original source code. When false, it returns the GraphNode ID
    /// @retval    exits Vector of IDs that specify which nodes (or blocks) are the kernel exits. Kernel exits are the destination nodes of edges that leave the kernel
    const std::set<uint64_t> getExits() const //bool block = false) const
    {
        std::set<uint64_t> exitNodes;
        for (const auto &node : nodes)
        {
            for (const auto &neighbor : node.neighbors)
            {
                if (nodes.find(neighbor.first) == nodes.end())
                {
                    // we've found an exit
                    exitNodes.insert(neighbor.first);
                }
            }
        }
        /*if( block )
        {
            std::set<uint64_t> exits;
            for( const auto& e : exitNodes )
            {
                exits.insert()
            }
        }*/
        return exitNodes;
    }
    /// @brief Returns the member blocks (from the source bitcode) of this kernel
    const std::set<uint64_t> getBlocks() const
    {
        std::set<uint64_t> blocks;
        for (const auto &node : nodes)
        {
            for (const auto &block : node.blocks)
            {
                blocks.insert(block.first);
            }
        }
        return blocks;
    }
    /// @brief Compares this kernel to another kernel by measuring node differences
    ///
    /// If two kernels are the same, 1 will be returned
    /// If two kernels are completely different, 0 will be returned
    /// If two kernels share some nodes, (compare shared) / (this size) will be returned
    /// TODO: if this object fully overlaps with compare, but compare contains other blocks, this will say that we fully match when we actually don't. Fix that
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
    /// Returns true if any node in the kernel can reach every other node in the kernel. False otherwise
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
    inline bool operator==(const Kernel &rhs) const
    {
        return rhs.KID == KID;
    }

private:
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