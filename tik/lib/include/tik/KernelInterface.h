#pragma once
#include <llvm/IR/BasicBlock.h>

namespace TraceAtlas::tik
{
    class KernelInterface
    {
    public:
        KernelInterface(int index, int64_t block);
        int Index;
        int64_t Block;
        bool operator<(const KernelInterface &b) const;
    };
} // namespace TraceAtlas::tik