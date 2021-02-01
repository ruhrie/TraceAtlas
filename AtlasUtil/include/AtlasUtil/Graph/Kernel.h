#pragma once
#include "AtlasUtil/Graph/Graph.h"
#include <AtlasUtil/Graph/GraphChecks.h>
#include <cfloat>
#include <set>
#include <vector>

enum class Legality
{
    /// All requirements are met
    Legal,
    /// The kernel is "strongly connected", meaning every node within the kernel graph can reach all other nodes
    RuleOne,
    /// The kernel does not overlap with any other kernels
    RuleTwo,
    /// Every node in the kernel is more probable to keep recurring than to exit
    RuleThree,
    /// If there is a hierarchy to two or more kernels, there must be a unique entrance to each of the children
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
        if (!IsStronglyConnected(Blocks, graph))
        {
            return Legality::RuleOne;
        }
        //requirement 2: no partial overlaps
        //check this last

        //requirement 3: more probable to stay inside than out
        // if any node has an edge that is more probable to leave the kernel than recur back into it, rule 3 is violated
        for (const auto &block : Blocks)
        {
            try
            {
                //get the max path
                uint64_t minBlock = 0;
                float prob = FLT_MAX;
                auto blockIndex = graph.LocationAlias.at(block);
                // find the maximum probability edge of this block and remember it
                for (int i = 0; i < graph.WeightMatrix.at(blockIndex).size(); i++)
                {
                    if (graph.WeightMatrix.at(blockIndex).at(i) < prob)
                    {
                        minBlock = i;
                        prob = graph.WeightMatrix.at(blockIndex).at(i);
                    }
                }
                // look at the destination node of the maximum probability edge, if it is outside the kernel then rule 3 is violated
                for (auto subBlock : graph.IndexAlias.at(minBlock))
                {
                    if (find(Blocks.begin(), Blocks.end(), subBlock) == Blocks.end()) //more probable to leave
                    {
                        return Legality::RuleThree;
                    }
                }
            }
            catch (std::exception &e)
            {
                spdlog::error("Exception thrown in IsLegal method: " + std::string(e.what()));
                return Legality::RuleThree;
            }
        }
        //requirement 4: a hierarchy must have a distinct entrance
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
            try
            {
                if (!difference.empty()) //there is a hierarchy (we already know it doesn't overlap partially by rule 2)
                {
                    int entranceCount = 0;
                    for (auto &block : difference)
                    {
                        uint64_t blockLoc = probGraph.LocationAlias.at(block);
                        for (int i = 0; i < probGraph.WeightMatrix.size(); i++)
                        {
                            float weight = probGraph.WeightMatrix.at(i).at(blockLoc);
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
            catch (std::exception &e)
            {
                spdlog::error("Exception thrown in IsLegal method: " + std::string(e.what()));
                return Legality::RuleFour;
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
    /// @brief Evaluates rules 1, 3 and 4 of a legal kernel
    /// @retval Returns the ratio of edges that are not shared (numerator) and shared (denominator). 0 means total overlap between the two, 1 means no common edges. -1 means the two kernels fused together are not strongly connected.
    float ScoreSimilarity(const GraphKernel &compare, const Graph<uint64_t> &graph, const Graph<float> &probGraph) const
    {
        //first check that they aren't hierarchical (If they are then why bother merging)
        //note that this part is asymmetric
        std::set<uint64_t> diffA;
        std::set<uint64_t> diffB;
        set_difference(Blocks.begin(), Blocks.end(), compare.Blocks.begin(), compare.Blocks.end(), std::inserter(diffA, diffA.begin()));
        set_difference(compare.Blocks.begin(), compare.Blocks.end(), Blocks.begin(), Blocks.end(), std::inserter(diffB, diffB.begin()));

        // Should return 0. This case means total overlap, which is indicated by a retval of 0
        if (diffB.empty())
        {
            return -1;
        }

        //second check that a fusion is strongly connected
        std::set<uint64_t> fusedSet;
        fusedSet.insert(Blocks.begin(), Blocks.end());
        fusedSet.insert(compare.Blocks.begin(), compare.Blocks.end());

        bool broke = false;
        if (!IsStronglyConnected(fusedSet, probGraph))
        {
            return -1;
        }

        //score is sum of non-matching edges over total edges of merged graph
        //optimal is 0, worst case is 1
        float numerator = 0;
        float denominator = 0;
        std::set<uint64_t> diff;
        set_difference(Blocks.begin(), Blocks.end(), compare.Blocks.begin(), compare.Blocks.end(), std::inserter(diff, diff.begin()));
        try
        {
            for (const auto &a : fusedSet)
            {
                auto A = graph.LocationAlias.at(a);
                for (const auto &b : fusedSet)
                {
                    auto B = graph.LocationAlias.at(b);
                    denominator += graph.WeightMatrix.at(A).at(B);
                    if (diff.find(a) != diff.end() || diff.find(b) != diff.end())
                    {
                        numerator += graph.WeightMatrix.at(A).at(B);
                    }
                }
            }
        }
        catch (std::exception &e)
        {
            spdlog::error("Exception thrown in ScoreSimilarity method: " + std::string(e.what()));
            return -1;
        }
        return numerator / denominator;
    }
};