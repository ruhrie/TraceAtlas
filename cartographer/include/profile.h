#pragma once
#include <llvm/IR/Module.h>
#include <map>
#include <set>
#include <vector>
std::map<std::string, std::map<std::string, std::map<std::string, int>>> ProfileKernels(std::map<int, std::set<int>> kernels, llvm::Module *M);

void ProfileBlock(llvm::BasicBlock *BB, std::map<int, std::map<std::string, int>> &rMap, std::map<int, std::map<std::string, int>> &cpMap);
