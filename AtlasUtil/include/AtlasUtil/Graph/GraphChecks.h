#pragma once
#include <AtlasUtil/Graph/Graph.h>
#include <algorithm>
#include <cstdint>
#include <vector>

inline bool IsStronglyConnected(std::set<uint64_t> elements, Graph<float> graph)
{
    using namespace std;
    for (auto e : elements)
    {
        set<uint64_t> reachable;
        set<uint64_t> visited;
        vector<uint64_t> toProcess;
        toProcess.push_back(e);
        while (!toProcess.empty())
        {
            auto c = toProcess.back();
            auto C = graph.LocationAlias[c];
            toProcess.pop_back();
            visited.insert(c);
            for (auto f : elements)
            {
                auto g = graph.LocationAlias[f];
                if (graph.NeighborMap[C].find(g) != graph.NeighborMap[C].end())
                {
                    reachable.insert(f);
                    if (find(toProcess.begin(), toProcess.end(), f) == toProcess.end() && visited.find(f) == visited.end())
                    {
                        toProcess.push_back(f);
                    }
                }
            }
        }
        if (elements != reachable)
        {
            return false;
        }
    }
    return true;
}