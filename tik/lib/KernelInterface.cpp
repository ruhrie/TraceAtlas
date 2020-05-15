#include "tik/KernelInterface.h"

namespace TraceAtlas::tik
{
    KernelInterface::KernelInterface(int index, int block)
    {
        Index = index;
        Block = block;
    }
    bool KernelInterface::operator<(const KernelInterface &b) const
    {
        return Block < b.Block;
    }
} // namespace TraceAtlas::tik
