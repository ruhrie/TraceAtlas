#pragma once
#include <llvm/IR/Module.h>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

std::set<std::set<int>> ExtractKernels(std::string sourceFile, std::set<std::set<int>> kernels, llvm::Module *bitcode);
