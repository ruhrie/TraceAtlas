#pragma once
#include <llvm/IR/BasicBlock.h>

namespace TraceAtlas::tik
{
    class KernelInterface
    {
    public:
        KernelInterface(int index, llvm::BasicBlock *block);
        int Index;
        llvm::BasicBlock *Block = nullptr;
        bool operator<(const KernelInterface &b) const;
    };
} // namespace TraceAtlas::tik