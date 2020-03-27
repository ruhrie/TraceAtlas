#pragma once
#include <llvm/IR/Module.h>
#include <map>
#include <set>
#include <vector>
std::map<std::string, std::map<std::string, std::map<std::string, int>>> ProfileKernels(std::map<int, std::set<int64_t>> kernels, llvm::Module *M);

void ProfileBlock(llvm::BasicBlock *BB, std::map<int64_t, std::map<std::string, uint64_t>> &rMap, std::map<int64_t, std::map<std::string, uint64_t>> &cpMap);
