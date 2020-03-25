#pragma once
#include <llvm/IR/Module.h>
#include <set>

namespace TypeThree
{
    std::set<std::set<int>> Process(std::set<std::set<int>> type25Kernels, llvm::Module *M);
} // namespace TypeThree
