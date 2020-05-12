#include "tik/KernelExit.h"

namespace TraceAtlas::tik
{
    KernelExit::KernelExit(int index, llvm::BasicBlock *target)
    {
        ExitIndex = index;
        Target = target;
    }
    bool KernelExit::operator<(const KernelExit &b) const
    {
        return Target < b.Target;
    }
} // namespace TraceAtlas::tik
