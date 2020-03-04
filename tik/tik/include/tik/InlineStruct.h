#pragma once
#include <llvm/IR/Instructions.h>
#include <vector>

struct InlineStruct
{
    llvm::Function *CalledFunction = NULL;
    std::vector<llvm::PHINode *> ArgNodes;
    llvm::SwitchInst *SwitchInstruction = NULL;
};