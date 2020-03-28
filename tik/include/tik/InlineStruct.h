#pragma once
#include <llvm/IR/Instructions.h>
#include <vector>

struct InlineStruct
{
    llvm::Function *CalledFunction = nullptr;
    std::vector<llvm::PHINode *> ArgNodes;
    llvm::SwitchInst *SwitchInstruction = nullptr;
    int currentIndex = 0;
    llvm::PHINode *branchPhi = nullptr;
    int phiIndex = 0;
    llvm::PHINode *returnPhi = nullptr;
    llvm::BasicBlock *entranceBlock = nullptr;
};