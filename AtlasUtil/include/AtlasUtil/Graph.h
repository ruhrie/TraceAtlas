#pragma once
#include <llvm/IR/Module.h>
#include <memory>
#include <stdint.h>
#include <vector>

template <class T>
class Graph
{
public:
    std::vector<std::vector<T>> WeightMatrix;
    std::map<uint64_t, std::vector<uint64_t>> IndexAlias; //node to block
    std::map<uint64_t, uint64_t> LocationAlias;           //block to node
};