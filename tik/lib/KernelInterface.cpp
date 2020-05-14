#include "tik/KernelInterface.h"

namespace TraceAtlas::tik
{
    KernelInterface::KernelInterface(int index, llvm::BasicBlock *target)
    {
        Index = index;
        Target = target;
    }
    bool KernelInterface::operator<(const KernelInterface &b) const
    {
        return Target < b.Target;
    }
} // namespace TraceAtlas::tik
