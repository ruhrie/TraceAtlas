#pragma once
#include <llvm/IR/BasicBlock.h>
#include <map>
#include <set>
#include <string>

extern bool noProgressBar;
extern bool blocksLabeled;
extern std::map<int, std::set<std::string>> blockLabelMap;
extern std::map<int, llvm::BasicBlock *> blockMap;