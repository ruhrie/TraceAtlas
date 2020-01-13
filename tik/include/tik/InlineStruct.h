#pragma once
#include <llvm/IR/Instructions.h>
#include <vector>

struct InlineStruct
{
    llvm::Function *CalledFunction;
    std::vector<llvm::PHINode *> ArgNodes;
    llvm::PHINode *ReturnNode;
};