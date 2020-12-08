#pragma once
#include "AtlasUtil/Graph/Graph.h"
#include <AtlasUtil/Graph/GraphChecks.h>
#include <cfloat>
#include <set>
#include <vector>

enum class Legality
{
    Legal,
    RuleOne,
    RuleTwo,
    RuleThree,
    RuleFour
};

class GraphKernel
{
public:
    GraphKernel(std::set<uint64_t> kernelBlocks)
    {
        Blocks = std::move(kernelBlocks);
    }
    GraphKernel(std::vector<uint64_t> kernelBlocks)
    {
        Blocks = std::set<uint64_t>(kernelBlocks.begin(), kernelBlocks.end());
    }
    GraphKernel() = default;
    bool operator<(const GraphKernel &x) const
    {
        return Blocks < x.Blocks;
    }
    bool operator!=(const GraphKernel &x) const
    {
        return Blocks != x.Blocks;
    }
    Legality IsLegal(const Graph<float> &graph, const std::set<GraphKernel> &kernels, const Graph<float> &probGraph) const
    {
        //requirement 1: is strongly connected
        if(!IsStronglyConnected(Blocks, graph))
        {
            return Legality::RuleOne;
        }
        //requirement 2: no partial overlaps
        //check this last

        //requirement 3: more probable to stay inside than out
        for (const auto &block : Blocks)
        {
            //get the max path
            uint64_t minBlock = 0;
            float prob = FLT_MAX;
            for (int i = 0; i < graph.WeightMatrix[block].size(); i++)
            {
                if (graph.WeightMatrix[block][i] < prob)
                {
                    minBlock = i;
                    prob = graph.WeightMatrix[block][i];
                }
            }
            if (find(Blocks.begin(), Blocks.end(), minBlock) == Blocks.end()) //more probable to leave
            {
                return Legality::RuleThree;
            }
        }
        //requirement 4: a hierarchy must have a distinct entrace
        //only enforce this on the child kernel, the parent is technically legal already
        for (auto &kComp : kernels)
        {
            std::set<uint64_t> intersection;
            set_intersection(kComp.Blocks.begin(), kComp.Blocks.end(), Blocks.begin(), Blocks.end(), std::inserter(intersection, intersection.begin()));
            if (intersection.empty())
            {
                continue;
            }
            std::set<uint64_t> difference;
            set_difference(kComp.Blocks.begin(), kComp.Blocks.end(), Blocks.begin(), Blocks.end(), std::inserter(difference, difference.begin()));
            if (!difference.empty()) //there is a hierarchy (we already know it doesn't overlap partially by rule 2)
            {
                int entranceCount = 0;
                for (auto &block : difference)
                {
                    uint64_t blockLoc = probGraph.LocationAlias.at(block);
                    for (int i = 0; i < probGraph.WeightMatrix.size(); i++)
                    {
                        float weight = probGraph.WeightMatrix[i][blockLoc];
                        if (weight != std::numeric_limits<float>::infinity() && !std::isnan(weight))
                        {
                            //this node is a predecessor, if it is an entrance it will not be in kComp
                            auto enterBlocks = probGraph.IndexAlias.at(i);
                            bool external = false;
                            for (auto entrance : enterBlocks)
                            {
                                if (kComp.Blocks.find(entrance) == kComp.Blocks.end())
                                {
                                    external = true;
                                    break;
                                }
                            }
                            if (external)
                            {
                                entranceCount += 1;
                            }
                        }
                    }
                }
                if (entranceCount == 0)
                {
                    return Legality::RuleFour;
                }
            }
        }

        //now rule 2
        for (auto &kComp : kernels)
        {
            std::set<uint64_t> intersection;
            set_intersection(kComp.Blocks.begin(), kComp.Blocks.end(), Blocks.begin(), Blocks.end(), std::inserter(intersection, intersection.begin()));
            if (!intersection.empty()) //at least part intersects
            {
                if (intersection != Blocks && intersection != kComp.Blocks) //check if a superset or a subset
                {
                    return Legality::RuleTwo;
                }
            }
        }

        //all clear, so is valid
        return Legality::Legal;
    }
    std::set<uint64_t> Blocks;
    float ScoreSimilarity(const GraphKernel &compare, const Graph<uint64_t> &graph, const Graph<float> &probGraph) const
    {
        //first check that they aren't hierarchical (If they are then why bother merging)
        //note that this part is asymetric
        std::set<uint64_t> diffA;
        std::set<uint64_t> diffB;
        set_difference(Blocks.begin(), Blocks.end(), compare.Blocks.begin(), compare.Blocks.end(), std::inserter(diffA, diffA.begin()));
        set_difference(compare.Blocks.begin(), compare.Blocks.end(), Blocks.begin(), Blocks.end(), std::inserter(diffB, diffB.begin()));

        if (diffB.empty())
        {
            return -1;
        }

        //second check that a fusion is strongly connected
        std::set<uint64_t> fusedSet;
        fusedSet.insert(Blocks.begin(), Blocks.end());
        fusedSet.insert(compare.Blocks.begin(), compare.Blocks.end());

        bool broke = false;
        if(!IsStronglyConnected(fusedSet, probGraph))
        {
            return -1;
        }

        //score is sum of non-matching edges over total edges of merged graph
        //optimal is 0, worst case is 1
        float numerator = 0;
        float denominator = 0;
        std::set<uint64_t> diff;
        set_difference(Blocks.begin(), Blocks.end(), compare.Blocks.begin(), compare.Blocks.end(), std::inserter(diff, diff.begin()));
        for (const auto &a : fusedSet)
        {
            for (const auto &b : fusedSet)
            {
                denominator += graph.WeightMatrix[a][b];
                if (diff.find(a) != diff.end() || diff.find(b) != diff.end())
                {
                    numerator += graph.WeightMatrix[a][b];
                }
            }
        }
        return numerator / denominator;
    }
};