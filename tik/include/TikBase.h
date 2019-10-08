#pragma once
#include <json.hpp>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Function.h>
#include <set>
class TikBase
{
public:
    llvm::BasicBlock *Body = NULL;
    llvm::BasicBlock *Init = NULL;
    llvm::Function *MemoryRead = NULL;
    llvm::Function *MemoryWrite = NULL;
    virtual nlohmann::json GetJson();
    TikBase();
    ~TikBase();
protected:
    llvm::Module *baseModule;
};