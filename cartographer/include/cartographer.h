#pragma once
#include <llvm/IR/BasicBlock.h>
#include <map>
#include <set>
#include <string>

extern llvm::cl::opt<bool> noBar;
extern llvm::cl::opt<float> threshold;
extern llvm::cl::opt<int> hotThreshold;
extern bool blocksLabeled;
extern std::map<int, std::set<std::string>> blockLabelMap;
extern std::map<int, llvm::BasicBlock *> blockMap;