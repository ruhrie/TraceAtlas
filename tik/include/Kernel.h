#pragma once
#include "TikBase.h"
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <string>
#include <vector>
class Kernel : public TikBase
{
public:
    Kernel(std::vector<int> basicBlocks, llvm::Module *M);
    ~Kernel();
    std::string Name;
    nlohmann::json GetJson();
    llvm::BasicBlock *Conditional = NULL;

private:
    llvm::Function *mainFunction = NULL;
    llvm::ValueToValueMapTy VMap;
    void Remap();
    void GetLoopInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetBodyInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetInitInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetMemoryFunctions(llvm::Module *m);
    std::vector<llvm::Instruction *> getInstructionPath(llvm::BasicBlock *start, std::vector<llvm::BasicBlock *> validBlocks, std::vector<llvm::Instruction *> currentSet = {});
};