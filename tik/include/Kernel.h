#pragma once
#include <json.hpp>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <string>
#include <vector>
class Kernel
{
public:
    Kernel(std::vector<int> basicBlocks, llvm::Module *M);
    ~Kernel();
    std::string Name;
    nlohmann::json GetJson();
    llvm::BasicBlock *Conditional = NULL;
    llvm::BasicBlock *ExitTarget = NULL;
    llvm::BasicBlock *Body = NULL;
    llvm::BasicBlock *Init = NULL;
    llvm::Function *MemoryRead = NULL;
    llvm::Function *MemoryWrite = NULL;

private:
    llvm::Function *KernelFunction = NULL;
    llvm::ValueToValueMapTy VMap;
    void Remap();
    void GetLoopInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetBodyInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetInitInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetExits(std::vector<llvm::BasicBlock *> blocks);
    void GetMemoryFunctions();
};