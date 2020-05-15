#include "tik/libtik.h"
#include <map>
#include <memory>
namespace TraceAtlas::tik
{
    llvm::Module *TikModule;
    std::map<int64_t, std::shared_ptr<CartographerKernel>> KernelMap;
    std::map<llvm::Function *, std::shared_ptr<CartographerKernel>> KfMap;
} // namespace TraceAtlas::tik