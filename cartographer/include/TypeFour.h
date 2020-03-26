#pragma once
#include <llvm/IR/Module.h>
#include <set>

namespace TypeFour
{
    std::set<llvm::BasicBlock *> GetReachable(llvm::BasicBlock *base, std::set<int> validBlocks);
    std::set<llvm::BasicBlock *> GetEntrances(std::set<llvm::BasicBlock *> blocks);
    std::set<std::set<int>> Process(std::set<std::set<int>> type3Kernels, llvm::Module *M);
    std::set<llvm::BasicBlock *> GetVisitable(llvm::BasicBlock *base, std::set<llvm::BasicBlock *> &validBlocks);
} // namespace TypeFour
