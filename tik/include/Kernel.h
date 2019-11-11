#pragma once
#include <json.hpp>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <string>
#include <vector>
#include <map>
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
    llvm::BasicBlock *Exit = NULL;
    llvm::Function *MemoryRead = NULL;
    llvm::Function *MemoryWrite = NULL;


private:
    llvm::Function *KernelFunction = NULL;
    llvm::ValueToValueMapTy VMap;
    llvm::Value* cond = NULL;
    std::map < llvm::Value*, llvm::GlobalValue* > GlobalMap;
    std::vector<llvm::Instruction *> toAdd;
    void Remap();
    void GetLoopInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetBodyInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetInitInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetExits(std::vector<llvm::BasicBlock *> blocks);
    void CreateExitBlock(void);
    void GetMemoryFunctions(void);
    void ConnectFunctions(void);

};