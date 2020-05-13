#pragma once
#include <llvm/IR/BasicBlock.h>

namespace TraceAtlas::tik
{
    class KernelExit
    {
    public:
        KernelExit(int index, llvm::BasicBlock *target);
        int ExitIndex;
        llvm::BasicBlock *Target = nullptr;
        llvm::BasicBlock *Destination = nullptr;
        bool operator<(const KernelExit &b) const;
    };
} // namespace TraceAtlas::tik