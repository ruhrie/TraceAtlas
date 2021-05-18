#include "Dijkstra.h"
#include "GraphNode.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <map>

using namespace std;
using namespace TraceAtlas::Cartographer;

struct DijkstraCompare
{
    using is_transparent = void;
    bool operator()(const DijkstraNode &lhs, const DijkstraNode &rhs) const
    {
        return lhs.distance < rhs.distance;
    }
} DCompare;

DijkstraNode::DijkstraNode(double d, uint64_t id, uint64_t p, NodeColor c)
{
    distance = d;
    NID = id;
    predecessor = p;
    color = c;
}

vector<uint64_t> TraceAtlas::Cartographer::Dijkstras(const set<GraphNode *, p_GNCompare> &nodes, uint64_t source, uint64_t sink)
{
    // maps a node ID to its dijkstra information
    map<uint64_t, DijkstraNode> DMap;
    for (const auto &node : nodes)
    {
        // initialize each dijkstra node to have infinite distance, itself as its predecessor, and the unvisited nodecolor
        DMap[node->NID] = DijkstraNode(INFINITY, node->NID, std::numeric_limits<uint64_t>::max(), NodeColor::White);
    }
    DMap[source] = DijkstraNode(0, source, std::numeric_limits<uint64_t>::max(), NodeColor::White);
    // priority queue that holds all newly discovered nodes. Minimum paths get priority
    // this deque gets sorted before each iteration, emulating the behavior of a priority queue, which is necessary because std::priority_queue does not support DECREASE_KEY operation
    deque<DijkstraNode> Q;
    Q.push_back(DMap[source]);
    while (!Q.empty())
    {
        // sort the priority queue
        std::sort(Q.begin(), Q.end(), DCompare);
        // for each neighbor of u, calculate the neighbors new distance
        if (nodes.find(Q.front().NID) != nodes.end())
        {
            for (const auto &neighbor : (*nodes.find(Q.front().NID))->neighbors)
            {
                /*spdlog::info("Priority Q has the following entries in this order:");
                for( const auto& entry : Q )
                {
                    spdlog::info(to_string(entry.NID));
                }*/
                if (neighbor.first == source)
                {
                    // we've found a loop
                    // the DMap distance will be 0 for the source node so we can't do a comparison of distances on the first go-round
                    // if the source doesnt yet have a predecessor then update its stats
                    if (DMap[source].predecessor == std::numeric_limits<uint64_t>::max())
                    {
                        DMap[source].predecessor = Q.front().NID;
                        DMap[source].distance = -log(neighbor.second.second) + DMap[Q.front().NID].distance;
                    }
                }
                if (-log(neighbor.second.second) + Q.front().distance < DMap[neighbor.first].distance)
                {
                    DMap[neighbor.first].predecessor = Q.front().NID;
                    DMap[neighbor.first].distance = -log(neighbor.second.second) + DMap[Q.front().NID].distance;
                    if (DMap[neighbor.first].color == NodeColor::White)
                    {
                        DMap[neighbor.first].color = NodeColor::Grey;
                        Q.push_back(DMap[neighbor.first]);
                    }
                    else if (DMap[neighbor.first].color == NodeColor::Grey)
                    {
                        // we have already seen this neighbor, it must be in the queue. We have to find its queue entry and update it
                        for (auto &node : Q)
                        {
                            if (node.NID == DMap[neighbor.first].NID)
                            {
                                node.predecessor = DMap[neighbor.first].predecessor;
                                node.distance = DMap[neighbor.first].distance;
                            }
                        }
                        std::sort(Q.begin(), Q.end(), DCompare);
                    }
                }
            }
        }
        DMap[Q.front().NID].color = NodeColor::Black;
        Q.pop_front();
    }
    // now construct the min path
    vector<uint64_t> newKernel;
    for (const auto &DN : DMap)
    {
        if (DN.first == sink)
        {
            if (DN.second.predecessor == std::numeric_limits<uint64_t>::max())
            {
                // there was no path found between source and sink
                return newKernel;
            }
            auto prevNode = DN.second.predecessor;
            newKernel.push_back((*nodes.find(prevNode))->NID);
            while (prevNode != source)
            {
                prevNode = DMap[prevNode].predecessor;
                newKernel.push_back((*nodes.find(prevNode))->NID);
            }
            break;
        }
    }
    return newKernel;
}