#include <GraphTransforms.h>
#include <cmath>

using namespace std;

Graph<float> ProbabilityTransform(Graph<uint64_t> input)
{
    Graph<float> result;

    for (int i = 0; i < input.WeightMatrix.size(); i++)
    {
        vector<float> newRow(input.WeightMatrix.size());
        float sum = 0.0f;
        
        for(uint64_t j : input.WeightMatrix[i])
        {
            sum += j;
        }
        
        for (int j = 0; j < input.WeightMatrix[i].size(); j++)
        {
            newRow[j] = -1 * log(input.WeightMatrix[i][j] / sum);
        }
        result.WeightMatrix.push_back(newRow);
    }
    result.LocationAlias = input.LocationAlias;
    result.IndexAlias = input.IndexAlias;

    return result;
}

Graph<float> GraphCollapse(Graph<float> base, const set<Kernel> &kernels)
{
    Graph<float> result;

    set<set<uint64_t>> mappedBlocks;
    for (const auto &kernel : kernels)
    {
        set<uint64_t> remappedKernel;
        for (const auto &block : kernel.Blocks)
        {
            remappedKernel.insert(base.LocationAlias[block]);
        }
        mappedBlocks.insert(remappedKernel);
    }

    //remove non-top level
    //needs testing
    set<set<uint64_t>> toRemove;
    for (const auto &k1 : mappedBlocks)
    {
        for (const auto &k2 : mappedBlocks)
        {
            if (k1 == k2)
            {
                continue;
            }
            set<uint64_t> difference;
            set_difference(k1.begin(), k1.end(), k2.begin(), k2.end(), std::inserter(difference, difference.begin()));
            if (difference.empty())
            {
                toRemove.insert(k2);
            }
        }
    }
    for (const auto &r : toRemove)
    {
        mappedBlocks.erase(r);
    }

    //build the new maps
    uint64_t newId = 0;
    for (const auto &priorIndex : base.IndexAlias)
    {
        auto pId = priorIndex.first;
        //check if this is in a new fusion
        bool fuse = false;
        for (const auto &k : mappedBlocks)
        {
            if (k.find(pId) != k.end())
            {
                bool found = false;
                for (const auto &block : k)
                {
                    if (result.LocationAlias.find(block) != result.LocationAlias.end()) //sus
                    {
                        //a match
                        found = true;
                        auto loc = result.LocationAlias[block];
                        result.IndexAlias[loc].push_back(pId);
                        result.LocationAlias[pId] = loc;
                        break;
                    }
                }
                if (!found)
                {
                    //never found one
                    result.IndexAlias[newId].push_back(pId);
                    result.LocationAlias[pId] = newId++;
                }
                fuse = true;
                break;
            }
        }
        if (!fuse)
        {
            result.IndexAlias[newId].push_back(pId);
            result.LocationAlias[pId] = newId++;
        }
    }

    return result;
}