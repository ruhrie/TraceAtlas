#pragma once
#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <map>
#include <set>
#include <filesystem>

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
            else if (auto arg = llvm::dyn_cast<llvm::Argument>(gv->getOperand(j)))
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

inline int64_t SetIDAndMap(llvm::Value *val, std::map<int64_t, llvm::Value *> &IDToValue, bool artificial = false)
{
    int64_t newID;
    if (artificial)
    {
        newID = IDState::Artificial;
    }
    else
    {
        newID = std::prev(IDToValue.end())->first + 1;
    }
    if (auto inst = llvm::dyn_cast<llvm::Instruction>(val))
    {
        llvm::MDNode *newMD = llvm::MDNode::get(inst->getParent()->getContext(), llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt64Ty(inst->getParent()->getContext()), (uint64_t)newID)));
        inst->setMetadata("ValueID", newMD);
        IDToValue[newID] = val;
        return newID;
    }
    return IDState::Uninitialized;
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

// the exports here represent the alloca mapped to an export
// therefore the debug information will capture the pointer operations
inline void DebugExports(llvm::Module *mod, std::map<int64_t, llvm::Value *> &IDToValue)
{
    mod->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
    auto DBuild = llvm::DIBuilder(*mod);
    std::string cwd = "test";//std::filesystem::current_path();
    auto uType = DBuild.createBasicType("debug", 8, 0);
    auto DFile = DBuild.createFile(mod->getSourceFileName(), cwd);
    //auto CompUnit = DBuild.createCompileUnit(llvm::dwarf::DW_LANG_C, DFile, "clang", false, ".", 0);
    auto ModMDN = llvm::MDNode::get(mod->getContext(), llvm::MDString::get(mod->getContext(), "TikSwapBitcode"));
    unsigned int lineNo = 0;
    //unsigned int newTag = 0;
    for (auto &f : *mod)
    {
        llvm::MDNode *FMDN;
        if (f.hasMetadata("KernelName"))
        {
            FMDN = f.getMetadata("KernelName");
        }
        else
        {
            FMDN = llvm::MDNode::get(f.getContext(), llvm::MDString::get(f.getContext(), f.getName()));
        }
        std::vector<llvm::Metadata*> ElTys;
        for( unsigned int i = 0; i < f.getNumOperands(); i++)
        {
            ElTys.push_back(uType);
        }
        auto ElTypeArray = DBuild.getOrCreateTypeArray(ElTys);
        auto SubTy = DBuild.createSubroutineType(ElTypeArray);
        auto SP = DBuild.createFunction(DFile, f.getName(), llvm::StringRef(), DFile, lineNo++, SubTy, 0);
        for (auto b = f.begin(); b != f.end(); b++)
        {
            for (auto it = b->begin(); it != b->end(); it++)
            {
                if (auto inst = llvm::dyn_cast<llvm::Instruction>(it))
                {
                    if (inst->getType()->getTypeID() != llvm::Type::VoidTyID)
                    {
                        /*llvm::MDNode *MDN;
                        std::string metaString;
                        int64_t ID;
                        if (it->getMetadata("ValueID") != nullptr)
                        {
                            MDN = it->getMetadata("ValueID");
                            if (auto mstring = llvm::dyn_cast<llvm::MDString>(MDN->getOperand(0)))
                            {
                                metaString = mstring->getString();
                                ID = std::stol(metaString);
                                if (ID == IDState::Artificial)
                                {
                                    ID = SetIDAndMap(llvm::cast<llvm::Value>(it), IDToValue);
                                }
                            }
                            else
                            {
                                ID = SetIDAndMap(llvm::cast<llvm::Value>(it), IDToValue);
                            }
                        }
                        else
                        {
                            ID = SetIDAndMap(llvm::cast<llvm::Value>(it), IDToValue);
                            MDN = llvm::MDNode::get(f.getContext(), llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt8Ty(f.getContext()), (uint64_t)ID)));
                        }
                        //llvm::DISubprogram::get(f.getContext(), FMDN, f.getName(), "test", )
                        //auto DScope = llvm::DILocalScope::get(f.getContext(), FMDN, ModMDN, lineNo++, 0);
                        auto DScope = llvm::DILocalScope::get(f.getContext(), FMDN);
                        auto DType = llvm::DIBasicType::get(f.getContext(), newTag++, it->getName());
                        //auto DV = DBuild.createAutoVariable(DScope, it->getName(), DFile, lineNo, DType);
                        auto DE = llvm::DIExpression::get(f.getContext(), 0);*/
                        auto DI = llvm::DILocation::get(f.getContext(), lineNo++, 0, SP);
                        inst->setDebugLoc(DI);
                        //DBuild.insertDeclare(llvm::cast<llvm::Value>(it), DV, DE, DI, llvm::cast<llvm::Instruction>(it));
                    }
                }
            }
        }
    }
}