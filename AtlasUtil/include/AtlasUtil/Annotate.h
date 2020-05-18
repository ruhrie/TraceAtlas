#pragma once
#include <llvm/IR/Constants.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>

inline void SetBlockID(llvm::BasicBlock *BB, int64_t i)
{
    llvm::MDNode *idNode = llvm::MDNode::get(BB->getContext(), llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt64Ty(BB->getContext()), (uint64_t)i)));
    BB->getFirstInsertionPt()->setMetadata("BlockID", idNode);
}

inline void SetValueID(llvm::Value *val, int64_t i)
{
    if (llvm::Instruction *inst = llvm::dyn_cast<llvm::Instruction>(val))
    {
        llvm::MDNode *idNode = llvm::MDNode::get(inst->getContext(), llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt64Ty(inst->getContext()), (uint64_t)i)));
        inst->setMetadata("ValueID", idNode);
    }
}

inline void Annotate(llvm::Function *F, uint64_t &startingIndex, uint64_t &valIndex)
{
    for (llvm::Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
    {
        SetBlockID(llvm::cast<llvm::BasicBlock>(BB), (int64_t)startingIndex);
        startingIndex++;
        for (auto bb = BB->begin(); bb != BB->end(); bb++)
        {
            SetValueID(llvm::cast<llvm::Value>(bb), (int64_t)valIndex);
            valIndex++;
        }
    }
}

inline void Annotate(llvm::Module *M)
{
    uint64_t index = 0;
    uint64_t valIndex = 0;
    for (auto mi = M->begin(); mi != M->end(); mi++)
    {
        llvm::Function *F = llvm::cast<llvm::Function>(mi);
        Annotate(F, index, valIndex);
    }
}

inline void CleanModule(llvm::Module *M)
{
    for (auto mi = M->begin(); mi != M->end(); mi++)
    {
        for (auto &fi : *mi)
        {
            std::vector<llvm::Instruction *> toRemove;
            for (auto bi = fi.begin(); bi != fi.end(); bi++)
            {
                auto v = llvm::cast<llvm::Instruction>(bi);
                if (auto ci = llvm::dyn_cast<llvm::DbgInfoIntrinsic>(v))
                {
                    toRemove.push_back(ci);
                }
                else
                {
                    llvm::SmallVector<std::pair<unsigned, llvm::MDNode *>, 1> MDs;
                    v->getAllMetadata(MDs);
                    for (auto MD : MDs)
                    {
                        v->setMetadata(MD.first, nullptr);
                    }
                }
            }
            for (auto r : toRemove)
            {
                r->eraseFromParent();
            }
        }
        auto *F = llvm::cast<llvm::Function>(mi);
        llvm::SmallVector<std::pair<unsigned, llvm::MDNode *>, 1> MDs;
        F->getAllMetadata(MDs);
        for (auto MD : MDs)
        {
            F->setMetadata(MD.first, nullptr);
        }
    }

    for (auto gi = M->global_begin(); gi != M->global_end(); gi++)
    {
        auto gv = llvm::cast<llvm::GlobalVariable>(gi);
        llvm::SmallVector<std::pair<unsigned, llvm::MDNode *>, 1> MDs;
        gv->getAllMetadata(MDs);
        for (auto MD : MDs)
        {
            gv->setMetadata(MD.first, nullptr);
        }
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

inline int64_t GetValueID(llvm::Value *val)
{
    int64_t result = -1;
    if (llvm::Instruction *first = llvm::dyn_cast<llvm::Instruction>(val))
    {
        if (llvm::MDNode *node = first->getMetadata("ValueID"))
        {
            auto ci = llvm::cast<llvm::ConstantInt>(llvm::cast<llvm::ConstantAsMetadata>(node->getOperand(0))->getValue());
            result = ci->getSExtValue();
        }
    }
    return result;
}