#pragma once
#include <llvm/IR/Module.h>
#include <set>

std::set<std::set<int>> RectifyKernel(std::set<std::set<int>> type3Kernels, llvm::Module *M);