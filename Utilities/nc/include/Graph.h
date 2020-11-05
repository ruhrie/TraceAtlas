#pragma once
#include <llvm/IR/Module.h>
#include <memory>
#include <stdint.h>
#include <vector>

struct Node
{
    std::vector<uint64_t> IDs;
};

struct Edge
{
    std::shared_ptr<Node> Source;
    std::shared_ptr<Node> Destination;
    uint64_t weight;
};

struct Graph
{
    std::vector<std::shared_ptr<Node>> Nodes;
    std::vector<std::shared_ptr<Edge>> Edges;
};

void BuildGraph(llvm::Module *M, std::vector<std::vector<uint64_t>> weights);
