#pragma once
#include "AtlasUtil/Graph.h"
#include <set>
#include <vector>
class Kernel
{
public:
    Kernel(std::set<uint64_t> kernelBlocks);
    Kernel(std::vector<uint64_t> kernelBlocks);
    bool operator<(const Kernel &x) const;
    bool IsLegal(const Graph<float> &graph, const std::set<Kernel> &kernels) const;
    std::set<uint64_t> Blocks;
};