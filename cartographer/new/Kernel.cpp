#include "Kernel.h"
#include "Dijkstra.h"

using namespace std;
using namespace TraceAtlas::Cartographer;

uint32_t Kernel::nextKID = 0;

Kernel::Kernel()
{
    KID = getNextKID();
    nodes = std::set<GraphNode, GNCompare>();
    Label = "";
}

Kernel::Kernel(uint32_t ID)
{
    KID = ID;
    nodes = std::set<GraphNode, GNCompare>();
    Label = "";
}

/// Returns the IDs of the kernel entrances
/// @retval    entrances Vector of IDs that specify which nodes (or blocks) are the kernel entrances. Kernel entrances are the source nodes of edges that enter the kernel
std::vector<GraphNode> Kernel::getEntrances() const
{
    std::vector<GraphNode> entrances;
    for (const auto &node : nodes)
    {
        for (const auto &pred : node.predecessors)
        {
            if (nodes.find(pred) == nodes.end())
            {
                // this node has a predecessor outside of the kernel, so it is considered an entrance node
                entrances.push_back(node);
                break;
            }
        }
    }
    return entrances;
}

/// @brief Returns the IDs of the kernel exits
///
/// @param[in] allNodes Set of all nodes in the control flow graph. Used to copy the nodes that are the destinations of edges that leave the kernel
/// @retval    exits Vector of IDs that specify which nodes (or blocks) are the kernel exits. Kernel exits are nodes that have a neighbor outside the kernel
std::vector<GraphNode> Kernel::getExits() const
{
    std::vector<GraphNode> exitNodes;
    for (const auto &node : nodes)
    {
        for (const auto &neighbor : node.neighbors)
        {
            if (nodes.find(neighbor.first) == nodes.end())
            {
                // we've found an exit
                exitNodes.push_back(node);
            }
        }
    }
    return exitNodes;
}

/// @brief Returns the member blocks (from the source bitcode) of this kernel
std::set<int64_t> Kernel::getBlocks() const
{
    std::set<int64_t> blocks;
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
std::set<int64_t> Kernel::Compare(const Kernel &compare) const
{
    std::set<int64_t> shared;
    for (const auto &compNode : compare.nodes)
    {
        if (nodes.find(compNode) != nodes.end())
        {
            for (const auto &block : compNode.blocks)
            {
                shared.insert(block.first);
            }
        }
    }
    return shared;
}

/// Returns true if any node in the kernel can reach every other node in the kernel. False otherwise
bool Kernel::FullyConnected() const
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

float Kernel::ExitProbability() const
{
    // our convention penalizes kernels with more than one exit
    // the probabilities of edges that leave the kernel are summed
    auto exits = getExits();
    float exitProbability = 0.0;
    for (const auto &exit : exits)
    {
        for (const auto &n : exit.neighbors)
        {
            if (nodes.find(n.first) == nodes.end())
            {
                exitProbability += (float)n.second.second;
            }
        }
    }
    return exitProbability;
}

inline bool Kernel::operator==(const Kernel &rhs) const
{
    return rhs.KID == KID;
}

uint32_t Kernel::getNextKID()
{
    return nextKID++;
}