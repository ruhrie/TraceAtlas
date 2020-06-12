#include "tik/libtik.h"
#include <map>
#include <memory>
namespace TraceAtlas::tik
{
    llvm::Module *TikModule;
    std::map<int64_t, std::shared_ptr<Kernel>> KernelMap;
    std::map<llvm::Function *, std::shared_ptr<Kernel>> KfMap;
} // namespace TraceAtlas::tik