#pragma once
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Function.h>
#include <vector>
#include <map>
#include <string>
#include <set>

void PrintVal(llvm::Value *val);
void PrintVal(llvm::Type *val);
std::string GetString(llvm::Value *v);
std::vector<std::string> GetStrings(llvm::BasicBlock *bb);
std::vector<std::string> GetStrings(std::set<llvm::Instruction *> instructions);
std::vector<std::string> GetStrings(std::vector<llvm::Instruction *> instructions);
std::map<std::string, std::vector<std::string>> GetStrings(llvm::Function *f);