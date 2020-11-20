#pragma once
#include <set>

class Kernel
{
public:
    Kernel(std::set<uint64_t> kernelBlocks);

private:
    std::set<uint64_t> Blocks;
};