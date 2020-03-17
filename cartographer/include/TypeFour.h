#pragma once
#include <llvm/IR/Module.h>
#include <set>

namespace TypeFour
{
    std::set<llvm::BasicBlock *> GetReachable(llvm::BasicBlock *base, std::set<int> validBlocks);
    std::set<std::set<int>> Process(std::set<std::set<int>> type3Kernels, llvm::Module *M);
} // namespace TypeFour
