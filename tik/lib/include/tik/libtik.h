#pragma once
#include "tik/Kernel.h"
#include <llvm/IR/Module.h>
#include <map>

namespace TraceAtlas::tik
{
    extern llvm::Module *TikModule;
    /// Maps an llvm function to a TraceAtlas::tik::Kernel object
    extern std::map<llvm::Function *, std::shared_ptr<Kernel>> KfMap;
    /// Maps a basic block ID to a TraceAtlas::tik::Kernel object
    extern std::map<int64_t, std::shared_ptr<Kernel>> KernelMap;
} // namespace TraceAtlas::tik
