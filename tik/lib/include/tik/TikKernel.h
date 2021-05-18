#include "tik/Kernel.h"

namespace TraceAtlas::tik
{
    /// @brief Constructs a kernel function object from a tik file
    ///
    /// The constructor of this class will either construct a TikKernel if the given argument is actually a kernel function, or will return an invalid TikKernel
    /// A function is only a valid TikKernel if it has Boundaries, Entrances, and Exits metadata attached to it.
    /// If the function is valid, the constructor will allocate a new function with the context of the input arg.
    class TikKernel : public Kernel
    {
    public:
        TikKernel() = default;
        TikKernel(llvm::Function *);
    };
} // namespace TraceAtlas::tik