#include "tik/Kernel.h"

namespace TraceAtlas::tik
{
    class TikKernel : public Kernel
    {
    public:
        TikKernel() = default;
        TikKernel(llvm::Function *kernFunc);
    };
} // namespace TraceAtlas::tik