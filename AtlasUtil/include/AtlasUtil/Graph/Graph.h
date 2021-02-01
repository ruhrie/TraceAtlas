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
    /// Maps a node to a map of neighbors that have a weight associated with the directed edge
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, T>> WeightMatrix;
    /// Maps a node ID in the WeightMatrix to a block ID from the original program
    /// The bitcode can contain chains of nodes that unconditionally branch sequentially. We transform these chains to be 1 node. This structure therefore maps a nodeID (that may be several blocks combined) to the blockID(s) it represents
    std::map<uint64_t, std::vector<uint64_t>> IndexAlias; //node to block
    /// Maps a blockID in the source program to a nodeID in the weightMatrix
    std::map<uint64_t, uint64_t> LocationAlias;           //block to node
    /// Maps a node to a set of neighbors
    std::map<uint64_t, std::set<uint64_t>> NeighborMap;
};