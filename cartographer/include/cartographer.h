#pragma once
#include <llvm/IR/BasicBlock.h>
#include <llvm/Support/CommandLine.h>
#include <map>
#include <set>
#include <string>

extern llvm::cl::opt<bool> noBar;
extern llvm::cl::opt<float> threshold;
extern llvm::cl::opt<int> hotThreshold;
extern bool blocksLabeled;
extern std::map<int64_t, std::set<std::string>> blockLabelMap;
extern std::map<int64_t, llvm::BasicBlock *> blockMap;
extern std::set<int64_t> ValidBlocks;
extern llvm::Module *bitcode;