#include "Dijkstra.h"
#include "AtlasUtil/Exceptions.h"
#include <algorithm>
#include <cmath>
#include <map>

using namespace std;

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
vector<uint64_t> Dijkstra(Graph<float> graph, uint64_t start, uint64_t end)
{
    vector<uint64_t> result;

    vector<DistanceTuple> distances(graph.WeightMatrix.size());
    map<uint64_t, vector<uint64_t>> paths;
    map<uint64_t, bool> visited;

    //init the distance graph
    for (int i = 0; i < graph.WeightMatrix.size(); i++)
    {
        distances[i] = DistanceTuple(i, INFINITY);
    }

    uint64_t current = start;
    bool done = false;
    while (!done)
    {
        float currentDist = 0;
        if (current != start)
        {
            for (auto &d : distances)
            {
                if (d.key == current)
                {
                    currentDist = d.distance;
                }
            }
        }
        auto &currentPath = paths[current];
        for (int i = 0; i < graph.WeightMatrix[current].size(); i++)
        {
            float newDist = currentDist + graph.WeightMatrix[current][i];
            DistanceTuple *compDistance = nullptr;
            for (auto &d : distances)
            {
                if (d.key == i)
                {
                    compDistance = &d;
                    break;
                }
            }
            if (compDistance == nullptr)
            {
                throw AtlasException("Failed to find branch")
            }
            if (newDist < compDistance->distance)
            {
                compDistance->distance = newDist;
                paths[i] = currentPath;
                if (paths[i].empty())
                {
                    paths[i].push_back(start);
                }
                paths[i].push_back(i);
            }
        }
        visited[current] = true;
        sort(distances.begin(), distances.end());
        bool found = false;
        for (auto &d : distances)
        {
            if (!visited[d.key])
            {
                current = d.key;
                found = true;
                break;
            }
        }
        if (!found)
        {
            break;
        }
        for (auto &d : distances)
        {
            if (d.key == end && d.distance != INFINITY)
            {
                done = true;
                result = paths[d.key];
                break;
            }
        }
    }

    return result;
}