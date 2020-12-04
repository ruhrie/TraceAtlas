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

//note, we assume this is a digraph
inline std::vector<uint64_t> Dijkstra(Graph<float> graph, uint64_t start, uint64_t end)
{
    uint64_t maxSize = graph.WeightMatrix.size();
    std::vector<float> dist(maxSize, std::numeric_limits<float>::infinity());
    std::vector<uint64_t> prev(maxSize, std::numeric_limits<uint64_t>::max());
    std::vector<bool> Q(maxSize, true);
    std::vector<uint64_t> result;
    uint64_t vCount = 0;
    for (auto i : graph.NeighborMap[start])
    {
        float &val = graph.WeightMatrix[start][i];
        dist[i] = val;
        prev[i] = start;
    }
    while (vCount != maxSize)
    {
        //select smallest here //real slow
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
        //unofficial
        if (u == std::numeric_limits<uint64_t>::max())
        {
            break;
        }
        Q[u] = false;
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
        for (auto v : graph.NeighborMap[u])
        {
            float &val = graph.WeightMatrix[u][v];
            auto alt = dist[u] + val;
            if (alt < dist[v])
            {
                dist[v] = alt;
                prev[v] = u;
            }
        }
        vCount++;
    }
    return result;
}