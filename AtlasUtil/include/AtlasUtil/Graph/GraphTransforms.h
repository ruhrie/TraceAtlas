#pragma once
#include "AtlasUtil/Graph/Graph.h"
#include "AtlasUtil/Graph/Kernel.h"
#include <set>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <vector>

inline Graph<float> ProbabilityTransform(Graph<uint64_t>& input)
{
    spdlog::trace("Building probability graph");
    Graph<float> result;

    for ( auto& i : input.WeightMatrix )
    {
        //std::vector<float> newRow(input.WeightMatrix.size());
        float sum = 0.0f;

        for (const auto& j : i.second)
        {
            sum += j.second;
        }

        for ( auto& j : i.second )
        {
            j.second = -1 * log(j.second / sum);
            //newRow[j] = -1 * log(input.WeightMatrix[i][j] / sum);
        }
        //result.WeightMatrix.push_back(newRow);
    }
    result.LocationAlias = input.LocationAlias;
    result.IndexAlias = input.IndexAlias;
    result.NeighborMap = input.NeighborMap;
    
    return result;
}

inline Graph<float> GraphCollapse(Graph<float> base, const std::set<GraphKernel> &kernels)
{
    spdlog::trace("Collapsing graph");
    Graph<float> result;

    std::set<std::set<uint64_t>> mappedBlocks;
    for (const auto &kernel : kernels)
    {
        std::set<uint64_t> remappedKernel;
        for (const auto &block : kernel.Blocks)
        {
            remappedKernel.insert(block);
        }
        mappedBlocks.insert(remappedKernel);
    }

    //remove non-top level
    //needs testing
    std::set<std::set<uint64_t>> toRemove;
    for (const auto &k1 : mappedBlocks)
    {
        for (const auto &k2 : mappedBlocks)
        {
            if (k1 == k2)
            {
                continue;
            }
            std::set<uint64_t> difference;
            set_difference(k2.begin(), k2.end(), k1.begin(), k1.end(), std::inserter(difference, difference.begin()));
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
        for (auto baseBlock : priorIndex.second)
        {
            //check if this is in a new fusion
            bool fuse = false;
            for (const auto &k : mappedBlocks)
            {
                if (k.find(baseBlock) != k.end())
                {
                    bool found = false;
                    for (const auto &block : k)
                    {
                        if (result.LocationAlias.find(block) != result.LocationAlias.end()) //sus
                        {
                            //a match
                            found = true;
                            auto loc = result.LocationAlias[block];
                            result.IndexAlias[loc].push_back(baseBlock);
                            result.LocationAlias[baseBlock] = loc;
                            break;
                        }
                    }
                    if (!found)
                    {
                        //never found one
                        result.IndexAlias[newId].push_back(baseBlock);
                        result.LocationAlias[baseBlock] = newId++;
                    }
                    fuse = true;
                    break;
                }
            }
            if (!fuse)
            {
                result.IndexAlias[newId].push_back(baseBlock);
                result.LocationAlias[baseBlock] = newId++;
            }
        }
    }

    //now that the dependencies are figured out we can populate the graph weights
    /*
    int popCount = result.IndexAlias.size();
    for (int i = 0; i < popCount; i++)
    {
        result.WeightMatrix.emplace_back(popCount);
    }
    */

    //merge the weights
    for (auto& i : base.WeightMatrix)
    {
        uint64_t x = result.LocationAlias[base.IndexAlias[i.first].front()];
        for ( auto& j : i.second)
        {
            uint64_t y = result.LocationAlias[base.IndexAlias[j.first].front()];
            //skip self cycles
            if (x != y)
            {
                result.WeightMatrix[x][y] += exp(-1 * j.second);
            }
        }
    }
    //normalize the probabilities
    for (uint64_t i = 0; i < result.WeightMatrix.size(); i++)
    {
        float sum = 0.0f;
        for (uint64_t j = 0; j < result.WeightMatrix.size(); j++)
        {
            sum += result.WeightMatrix[i][j];
        }
        for (uint64_t j = 0; j < result.WeightMatrix.size(); j++)
        {
            result.WeightMatrix[i][j] /= sum;
        }
    }
    //relogify them
    for (uint64_t i = 0; i < result.WeightMatrix.size(); i++)
    {
        for (uint64_t j = 0; j < result.WeightMatrix.size(); j++)
        {
            result.WeightMatrix[i][j] = -1 * log(result.WeightMatrix[i][j]);
        }
    }

    //remake neighbor map
    for (uint64_t i = 0; i < result.WeightMatrix.size(); i++)
    {
        for (uint64_t j = 0; j < result.WeightMatrix.size(); j++)
        {
            float &val = result.WeightMatrix[i][j];
            if (!std::isnan(val) && !std::isinf(val))
            {
                result.NeighborMap[i].insert(j);
            }
        }
    }

    return result;
}

/// @brief Transforms an input CFG into a minimal version of itself
///
/// 1.) Removes "no-neighbor" edges, an edge between two nodes that has 0 probability
/// 2.) TODO: Trivially merges chains of nodes connected sequentially by unconditional branches
inline Graph<uint64_t> CompressGraph(Graph<uint64_t> base)
{
    spdlog::trace("Compressing graph");
    //first remove no neighbor edges
    std::vector<uint64_t> indexesToRemove;
    for (const auto& i : base.WeightMatrix)
    {
        if (base.NeighborMap.find(i.first) == base.NeighborMap.end())
        {
            bool connected = false;
            for (const auto& j : i.second)
            {
                if (j.second != 0)
                {
                    connected = true;
                    break;
                }
            }
            if (!connected)
            {
                indexesToRemove.push_back(i.first);
            }
        }
    }
    Graph<uint64_t> noNeighborGraph;
    uint64_t newSize = base.WeightMatrix.size() - indexesToRemove.size();
    uint64_t j = 0;
    uint64_t m = 0;
    for (uint64_t i = 0; i < base.WeightMatrix.size(); i++)
    {
        if (find(indexesToRemove.begin(), indexesToRemove.end(), i) == indexesToRemove.end())
        {
            //std::vector<uint64_t> subEntries(newSize);
            uint64_t l = 0;
            for (uint64_t k = 0; k < base.WeightMatrix[i].size(); k++)
            {
                if (find(indexesToRemove.begin(), indexesToRemove.end(), k) == indexesToRemove.end())
                {
                    noNeighborGraph.WeightMatrix[m][l++] = base.WeightMatrix[i][k];
                    //subEntries[l++] = base.WeightMatrix[i][k];
                }
            }
            //noNeighborGraph.WeightMatrix.push_back(subEntries);
            noNeighborGraph.IndexAlias[j].push_back(i);
            noNeighborGraph.LocationAlias[i] = j;
            j++;
            m++;
        }
    }
    for (uint64_t i = 0; i < noNeighborGraph.WeightMatrix.size(); i++)
    {
        auto index = noNeighborGraph.IndexAlias[i][0];
        std::set<uint64_t> subNeighbor;
        for (auto k : base.NeighborMap[index])
        {
            subNeighbor.insert(noNeighborGraph.LocationAlias[k]);
        }
        noNeighborGraph.NeighborMap[i] = subNeighbor;
    }

    //then remove absolute edges
    return noNeighborGraph;
}