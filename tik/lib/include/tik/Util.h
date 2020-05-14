#pragma once
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace TraceAtlas::tik
{
    std::string GetString(llvm::Value *v);
    std::vector<std::string> GetStrings(llvm::BasicBlock *bb);
    std::vector<std::string> GetStrings(std::set<llvm::Instruction *> instructions);
    std::vector<std::string> GetStrings(std::vector<llvm::Instruction *> instructions);
    std::map<std::string, std::vector<std::string>> GetStrings(llvm::Function *f);
    std::set<llvm::BasicBlock *> GetReachable(llvm::BasicBlock *base, std::set<int64_t> validBlocks);
    bool IsSelfReachable(llvm::BasicBlock *base, const std::set<int64_t> &validBlocks);
    bool IsReachable(llvm::BasicBlock *base, llvm::BasicBlock *target, const std::set<int64_t> &validBlocks);
    std::set<llvm::BasicBlock *> GetEntrances(std::set<llvm::BasicBlock *> &blocks);
    std::set<llvm::BasicBlock *> GetExits(std::set<llvm::BasicBlock *> blocks);
    std::set<llvm::BasicBlock *> GetExits(llvm::Function *F);
    std::set<llvm::BasicBlock *> GetConditionals(const std::set<llvm::BasicBlock *> &blocks, const std::set<int64_t> &validBlocks);
    bool HasEpilogue(const std::set<llvm::BasicBlock *> &blocks, const std::set<int64_t> &validBlocks);
} // namespace TraceAtlas::tik
