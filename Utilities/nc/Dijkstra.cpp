#include <Dijkstra.h>
#include <algorithm>
#include <map>
#include <math.h>

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
    bool operator<(const DistanceTuple &x)
    {
        return distance < x.distance;
    }
};

//note, we assume this is a digraph
vector<uint64_t> Dijkstra(vector<vector<float>> graph, uint64_t start, uint64_t end)
{
    vector<uint64_t> result;

    vector<DistanceTuple> distances;
    map<uint64_t, vector<uint64_t>> paths;
    map<uint64_t, bool> visited;

    //init the distance graph
    for (int i = 0; i < graph.size(); i++)
    {
        distances.push_back(DistanceTuple(i, INFINITY));
    }

    uint64_t current = start;
    bool done = false;
    while (!done)
    {
        float currentDist;
        if (current == start)
        {
            currentDist = 0;
        }
        else
        {
            for (auto &d : distances)
            {
                if(d.key == current)
                {
                    currentDist = d.distance;
                }
            }
        }
        auto &currentPath = paths[current];
        for (int i = 0; i < graph[current].size(); i++)
        {
            float newDist = currentDist + graph[current][i];
            DistanceTuple *compDistance;
            for (auto &d : distances)
            {
                if (d.key == i)
                {
                    compDistance = &d;
                    break;
                }
            }

            if (newDist < compDistance->distance)
            {
                compDistance->distance = newDist;
                paths[i] = currentPath;
                if(paths[i].size() == 0)
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