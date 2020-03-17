#pragma once
#include <llvm/IR/Module.h>
#include <set>
#include <string>

namespace TypeThree
{
    std::set<std::set<int>> Process(std::set<std::set<int>> blocks, llvm::Module *M);
}
