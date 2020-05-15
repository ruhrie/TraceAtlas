#pragma once
#include "tik/CartographerKernel.h"
#include <llvm/IR/Module.h>
#include <map>

namespace TraceAtlas::tik
{
    extern llvm::Module *TikModule;
    extern std::map<llvm::Function *, std::shared_ptr<CartographerKernel>> KfMap;
    extern std::map<int64_t, std::shared_ptr<CartographerKernel>> KernelMap;
} // namespace TraceAtlas::tik
