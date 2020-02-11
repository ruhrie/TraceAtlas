#pragma once
#include <map>
#include <set>
#include <vector>
#include <llvm/IR/Module.h>
std::map<int, std::map<std::string, int>> ProfileKernels(std::map<int, std::set<int>> kernels, llvm::Module *M);

void ProfileBlock(llvm::BasicBlock *BB);
