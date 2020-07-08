#pragma once
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>

/// @brief Enumerate the different states of ValueID and BlockID
///
/// A ValueID or BlockID can be in three different states:
/// -2 -> Uninitialized
/// -1 -> Artificial (injected by tik)
enum IDState : int64_t
{
    Uninitialized = -2,
    Artificial = -1
};

inline void SetBlockID(llvm::BasicBlock *BB, int64_t i)
{
    llvm::MDNode *idNode = llvm::MDNode::get(BB->getContext(), llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt64Ty(BB->getContext()), (uint64_t)i)));
    BB->getFirstInsertionPt()->setMetadata("BlockID", idNode);
}

inline void SetValueIDs(llvm::Value *val, uint64_t &i)
{
    if (llvm::Instruction *inst = llvm::dyn_cast<llvm::Instruction>(val))
    {
        llvm::MDNode *idNode = llvm::MDNode::get(inst->getContext(), llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt64Ty(inst->getContext()), i)));
        std::string metaKind = "ValueID";
        if (inst->getMetadata(metaKind) == nullptr)
        {
            inst->setMetadata("ValueID", idNode);
            i++;
        }
        else
        {
            return;
        }
        for (unsigned int j = 0; j < inst->getNumOperands(); j++)
        {
            if (auto use = llvm::dyn_cast<llvm::User>(inst->getOperand(j)))
            {
                SetValueIDs(llvm::cast<llvm::Value>(use), i);
            }
            else if (auto arg = llvm::dyn_cast<llvm::Argument>(inst->getOperand(j)))
            {
                SetValueIDs(llvm::cast<llvm::Value>(arg), i);
            }
        }
    }
    else if (auto gv = llvm::dyn_cast<llvm::GlobalObject>(val))
    {
        llvm::MDNode *gvNode = llvm::MDNode::get(gv->getContext(), llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt64Ty(gv->getContext()), i)));
        std::string metaKind = "ValueID";
        if (gv->getMetadata(metaKind) == nullptr)
        {
            gv->setMetadata("ValueID", gvNode);
            i++;
        }
        else
        {
            return;
        }

        for (unsigned int j = 0; j < gv->getNumOperands(); j++)
        {
            if (auto use = llvm::dyn_cast<llvm::User>(gv->getOperand(j)))
            {
                SetValueIDs(llvm::cast<llvm::Value>(use), i);
            }
            else if (auto arg = llvm::dyn_cast<llvm::Argument>(inst->getOperand(j)))
            {
                SetValueIDs(llvm::cast<llvm::Value>(arg), i);
            }
        }
    }
    else if (auto arg = llvm::dyn_cast<llvm::Argument>(val))
    {
        // find the arg index in the parent function call and append metadata to that parent (arg0->valueID)
        auto func = arg->getParent();
        int index = 0;
        bool found = false;
        for (auto j = func->arg_begin(); j != func->arg_end(); j++)
        {
            auto funcArg = llvm::cast<llvm::Argument>(j);
            if (funcArg == arg)
            {
                found = true;
                break;
            }
            index++;
        }
        if (!found)
        {
            return;
        }
        std::string metaKind = "ArgId" + std::to_string(index);
        if (func->getMetadata(metaKind) == nullptr)
        {
            llvm::MDNode *argNode = llvm::MDNode::get(func->getContext(), llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt64Ty(func->getContext()), i)));
            func->setMetadata(metaKind, argNode);
            i++;
        }
        else
        {
            // already seen this arg
            return;
        }
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
            SetValueIDs(llvm::cast<llvm::Value>(bb), valIndex);
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
    int64_t result = IDState::Uninitialized;
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
    int64_t result = IDState::Uninitialized;
    if (llvm::Instruction *first = llvm::dyn_cast<llvm::Instruction>(val))
    {
        if (llvm::MDNode *node = first->getMetadata("ValueID"))
        {
            auto ci = llvm::cast<llvm::ConstantInt>(llvm::cast<llvm::ConstantAsMetadata>(node->getOperand(0))->getValue());
            result = ci->getSExtValue();
        }
    }
    else if (auto second = llvm::dyn_cast<llvm::GlobalObject>(val))
    {
        if (llvm::MDNode *node = second->getMetadata("ValueID"))
        {
            auto ci = llvm::cast<llvm::ConstantInt>(llvm::cast<llvm::ConstantAsMetadata>(node->getOperand(0))->getValue());
            result = ci->getSExtValue();
        }
    }
    else if (auto third = llvm::dyn_cast<llvm::Argument>(val))
    {
        auto func = third->getParent();
        int index = 0;
        bool found = false;
        for (auto j = func->arg_begin(); j != func->arg_end(); j++)
        {
            auto funcArg = llvm::cast<llvm::Argument>(j);
            if (funcArg == third)
            {
                found = true;
                break;
            }
            index++;
        }
        if (!found)
        {
            return result;
        }
        std::string metaKind = "ArgId" + std::to_string(index);
        if (auto node = func->getMetadata(metaKind))
        {
            auto ci = llvm::cast<llvm::ConstantInt>(llvm::cast<llvm::ConstantAsMetadata>(node->getOperand(0))->getValue());
            result = ci->getSExtValue();
        }
    }
    return result;
}