#pragma once
#include <llvm/IR/Instructions.h>
#include <vector>

struct InlineStruct
{
    llvm::Function *CalledFunction = NULL;
    std::vector<llvm::PHINode *> ArgNodes;
    llvm::SwitchInst *SwitchInstruction = NULL;
    int currentIndex = 0;
    llvm::PHINode *branchPhi = NULL;
    int phiIndex = 0;
    llvm::PHINode *returnPhi = NULL;
    llvm::BasicBlock *entranceBlock = NULL;
};