#pragma once
#include <llvm/IR/Module.h>
#include <set>
#include <string>

std::set<std::set<int>> SmoothKernel(std::set<std::set<int>> blocks, llvm::Module *M);
