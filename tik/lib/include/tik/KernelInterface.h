#pragma once
#include <llvm/IR/BasicBlock.h>

namespace TraceAtlas::tik
{
    class KernelInterface
    {
    public:
        KernelInterface(int index, llvm::BasicBlock *target);
        int Index;
        llvm::BasicBlock *Target = nullptr;
        bool operator<(const KernelInterface &b) const;
    };
} // namespace TraceAtlas::tik