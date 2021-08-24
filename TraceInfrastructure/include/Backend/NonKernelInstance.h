#pragma once
#include "Backend/CodeInstance.h"
#include "Backend/NonKernel.h"

class NonKernelInstance : public CodeInstance
{
public:
    std::set<uint64_t> blocks;
    uint64_t firstBlock;
    NonKernel *nk;
    NonKernelInstance(uint64_t firstBlock);
};
