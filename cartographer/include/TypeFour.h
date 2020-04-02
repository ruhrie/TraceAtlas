#pragma once
#include <llvm/IR/Module.h>
#include <set>

namespace TypeFour
{
    std::set<llvm::BasicBlock *> GetReachable(llvm::BasicBlock *base, std::set<int64_t> validBlocks);
    std::set<std::set<int64_t>> Process(const std::set<std::set<int64_t>> &type3Kernels);
    std::set<llvm::BasicBlock *> GetVisitable(llvm::BasicBlock *base, std::set<llvm::BasicBlock *> &validBlocks);
} // namespace TypeFour
