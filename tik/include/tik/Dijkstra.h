#pragma once
#include <llvm/IR/BasicBlock.h>
#include <map>
#include <set>

std::map<llvm::BasicBlock *, int> SolveDijkstraBack(std::set<llvm::BasicBlock *> exits, std::set<llvm::BasicBlock *> blocks);
std::map<llvm::BasicBlock *, int> SolveDijkstraFront(std::set<llvm::BasicBlock *> entrances, std::set<llvm::BasicBlock *> blocks);

std::map<llvm::BasicBlock *, int> DijkstraIII(std::set<llvm::BasicBlock *> entrances, std::set<llvm::BasicBlock *> blocks);