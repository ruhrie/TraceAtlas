#pragma once
#include <llvm/IR/Constants.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>

inline void SetBlockID(llvm::BasicBlock *BB, uint64_t i)
{
    llvm::MDNode *idNode = llvm::MDNode::get(BB->getContext(), llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt64Ty(BB->getContext()), i)));
    BB->getFirstInsertionPt()->setMetadata("BlockID", idNode);
}

inline void Annotate(llvm::Function *F, uint64_t &startingIndex)
{
    for (llvm::Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
    {
        SetBlockID(llvm::cast<llvm::BasicBlock>(BB), startingIndex++);
    }
}

inline void Annotate(llvm::Module *M)
{
    uint64_t index = 0;
    for (auto mi = M->begin(); mi != M->end(); mi++)
    {
        llvm::Function *F = llvm::cast<llvm::Function>(mi);
        Annotate(F, index);
    }
}

inline int64_t GetBlockID(llvm::BasicBlock *BB)
{
    int64_t result = -1;
    if (BB->empty())
    {
        return result;
    }
    llvm::Instruction *first = llvm::cast<llvm::Instruction>(BB->getFirstInsertionPt());
    if (llvm::MDNode *node = first->getMetadata("BlockID"))
    {
        auto ci = llvm::cast<llvm::ConstantInt>(llvm::cast<llvm::ConstantAsMetadata>(node->getOperand(0))->getValue());
        result = ci->getSExtValue();
    }
    return result;
}
