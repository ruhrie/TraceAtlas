#pragma once
#include <llvm/IR/BasicBlock.h>

namespace TraceAtlas::tik
{
    class KernelInterface
    {
    public:
        KernelInterface(int index, int block);
        int Index;
        int Block;
        bool operator<(const KernelInterface &b) const;
    };
} // namespace TraceAtlas::tik