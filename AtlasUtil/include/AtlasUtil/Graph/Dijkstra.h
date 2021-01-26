#pragma once
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Graph/Graph.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

struct DistanceTuple
{
    uint64_t key;
    float distance = INFINITY;
    DistanceTuple(uint64_t k, float d)
    {
        key = k;
        distance = d;
    }
    DistanceTuple()
    {
        key = -1;
    }
    bool operator<(const DistanceTuple &x) const
    {
        return distance < x.distance;
    }
};

/// @brief Uses dijkstra's algorithm to find the minimum probability cycle between start and end. We assume graph is a digraph
///
/// @param graph Probability graph matrix
/// @param start Source node
/// @param end   Sink node
/// @retval      List of node IDs that map the shortest cycle
inline std::vector<uint64_t> Dijkstra(const Graph<float> &graph, uint64_t start, uint64_t end)
{
    // count of all nodes in the graph
    uint64_t maxSize = graph.WeightMatrix.size();
    // list of distance values between the source node and an arbitrary node in the graph
    std::vector<float> dist(maxSize, std::numeric_limits<float>::infinity());
    // maps an index in dist to the source node ID in which the distance was measured
    std::vector<uint64_t> prev(maxSize, std::numeric_limits<uint64_t>::max());
    // records which nodes have already been analyzed
    std::vector<bool> Q(maxSize, true);
    // vector containing a minimum probability cycle between source and sink
    std::vector<uint64_t> result;
    // counts the number of nodes already visited
    uint64_t vCount = 0;
    // sanity check
    if (graph.NeighborMap.find(start) == graph.NeighborMap.end())
    {
        return result;
    }
    // for each neighbor of the source node, assign the distance to be the probability edge between source and neighbor
    for (auto i : graph.NeighborMap.at(start))
    {
        float val = graph.WeightMatrix.at(start).at(i);
        dist[i] = val;
        prev[i] = start;
    }
    while (vCount != maxSize)
    {
        //select smallest here //real slow
        // find the minimum probability neighbor
        uint64_t u = std::numeric_limits<uint64_t>::max();
        float d = INFINITY;
        for (int i = 0; i < maxSize; i++)
        {
            if (dist[i] < d && Q[i])
            {
                d = dist[i];
                u = i;
            }
        }
        // if no non-zero probability neighbors exist we're done
        if (u == std::numeric_limits<uint64_t>::max())
        {
            break;
        }
        // mark this node as visited
        Q[u] = false;
        // if our minimum distance neighbor is our sink node, we have found a minimum path, so record the result
        if (u == end)
        {
            while (u != std::numeric_limits<uint64_t>::max())
            {
                result.push_back(prev[u]);
                u = prev[u];
                if (u == start)
                {
                    break;
                }
            }
            std::reverse(result.begin(), result.end());
            break;
        }
        //for neighor of u
        // not sure why this check is necessary
        if (graph.NeighborMap.find(u) != graph.NeighborMap.end())
        {
            // for each neighbor of the current node
            for (auto v : graph.NeighborMap.at(u))
            {
                // if a lesser probability edge is found between this node and a neighbor, update the distance matrix
                float val = graph.WeightMatrix.at(u).at(v);
                auto alt = dist[u] + val;
                if (alt < dist[v])
                {
                    dist[v] = alt;
                    prev[v] = u;
                }
            }
        }
        vCount++;
    }
    return result;
}