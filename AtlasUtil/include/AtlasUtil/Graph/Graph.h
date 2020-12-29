#pragma once
#include <cstdint>
#include <llvm/IR/Module.h>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

template <class T>
class Graph
{
public:
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, T>> WeightMatrix;
    std::map<uint64_t, std::vector<uint64_t>> IndexAlias; //node to block
    std::map<uint64_t, uint64_t> LocationAlias;           //block to node
    std::map<uint64_t, std::set<uint64_t>> NeighborMap;
};