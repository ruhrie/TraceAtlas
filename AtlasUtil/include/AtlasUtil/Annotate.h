#pragma once
#include "AtlasUtil/Exceptions.h"
#include <filesystem>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Verifier.h>
#include <map>
#include <set>
#include <spdlog/spdlog.h>
#include <unistd.h>

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

inline uint64_t TraceAtlasIndex = 0;
inline uint64_t TraceAtlasValueIndex = 0;

inline void Annotate(llvm::Module *M)
{
    for (auto mi = M->begin(); mi != M->end(); mi++)
    {
        llvm::Function *F = llvm::cast<llvm::Function>(mi);
        Annotate(F, TraceAtlasIndex, TraceAtlasValueIndex);
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
    auto *first = llvm::cast<llvm::Instruction>(BB->getFirstInsertionPt());
    if (llvm::MDNode *node = first->getMetadata("BlockID"))
    {
        auto ci = llvm::cast<llvm::ConstantInt>(llvm::cast<llvm::ConstantAsMetadata>(node->getOperand(0))->getValue());
        result = ci->getSExtValue();
    }
    return result;
}

inline int64_t GetValueID(const llvm::Value *val)
{
    int64_t result = IDState::Uninitialized;
    if (auto *first = llvm::dyn_cast<llvm::Instruction>(val))
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

inline void RecurseThroughOperands(llvm::Value *val, std::map<int64_t, llvm::Value *> &IDToValue)
{
    if (auto inst = llvm::dyn_cast<llvm::Instruction>(val))
    {
        if (GetValueID(inst) < 0)
        {
            throw AtlasException("Found an instruction that did not have a ValueID.");
        }
        if (IDToValue.find(GetValueID(val)) == IDToValue.end())
        {
            IDToValue[GetValueID(val)] = val;
        }
        else
        {
            return;
        }
        for (unsigned int i = 0; i < inst->getNumOperands(); i++)
        {
            if (auto subVal = llvm::dyn_cast<llvm::Value>(inst->getOperand(i)))
            {
                RecurseThroughOperands(subVal, IDToValue);
            }
        }
    }
    else if (auto go = llvm::dyn_cast<llvm::GlobalObject>(val))
    {
        if (GetValueID(go) < 0)
        {
            throw AtlasException("Found a global object that did not have a ValueID.");
        }
        if (IDToValue.find(GetValueID(val)) == IDToValue.end())
        {
            IDToValue[GetValueID(go)] = val;
        }
        else
        {
            return;
        }
        for (unsigned int i = 0; i < go->getNumOperands(); i++)
        {
            if (auto subVal = llvm::dyn_cast<llvm::Value>(go->getOperand(i)))
            {
                RecurseThroughOperands(subVal, IDToValue);
            }
        }
    }
    else if (auto arg = llvm::dyn_cast<llvm::Argument>(val))
    {
        if (GetValueID(arg) < 0)
        {
            throw AtlasException("Found a global object that did not have a ValueID.");
        }
        if (IDToValue.find(GetValueID(val)) == IDToValue.end())
        {
            IDToValue[GetValueID(arg)] = val;
        }
        else
        {
            return;
        }
    }
}

inline void InitializeIDMaps(llvm::Module *M, std::map<int64_t, llvm::BasicBlock *> &IDToBlock, std::map<int64_t, llvm::Value *> &IDToValue)
{
    for (auto &F : *M)
    {
        for (auto BB = F.begin(); BB != F.end(); BB++)
        {
            auto *block = llvm::cast<llvm::BasicBlock>(BB);
            if ((GetBlockID(block) >= 0) && (IDToBlock[GetBlockID(block)] == nullptr))
            {
                IDToBlock[GetBlockID(block)] = block;
            }
            for (auto it = block->begin(); it != block->end(); it++)
            {
                auto inst = llvm::cast<llvm::Instruction>(it);
                if (auto val = llvm::dyn_cast<llvm::Value>(inst))
                {
                    RecurseThroughOperands(val, IDToValue);
                }
            }
        }
    }
}

inline void findLine(const std::vector<std::string> &modLines, const std::string &name, unsigned int &lineNo, bool inst = false)
{
    for (unsigned int i = lineNo; i < modLines.size(); i++)
    {
        if (inst)
        {
            if (modLines[i - 1].find("!BlockID ") != std::string::npos)
            {
                lineNo = i;
                return;
            }
            else if (modLines[i - 1].find("Function Attrs") != std::string::npos)
            {
                // end of the line, return
                lineNo = i;
                return;
            }
        }
        else if ((modLines[i - 1].find("define") != std::string::npos) && (modLines[i - 1].find(name) != std::string::npos))
        {
            lineNo = i;
            return;
        }
    }
    // we couldn't find a definition. try again
    if (inst)
    {
        return;
    }
    lineNo = 1;
    findLine(modLines, name, lineNo);
}

// the exports here represent the alloca mapped to an export
// therefore the debug information will capture the pointer operations
inline void DebugExports(llvm::Module *mod, const std::string &fileName)
{
    if (mod->getModuleFlag("Debug Info Version") == nullptr)
    {
        mod->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
    }
    auto DBuild = llvm::DIBuilder(*mod);
    std::string cwd = get_current_dir_name();
    auto uType = DBuild.createBasicType("export", 64, llvm::dwarf::DW_ATE_address, llvm::DINode::DIFlags::FlagArtificial);
    auto DFile = DBuild.createFile(fileName, cwd);
    DBuild.createCompileUnit(llvm::dwarf::DW_LANG_C, DFile, "clang", false, ".", 0);
    unsigned int lineNo = 1;
    std::string strDump;
    llvm::raw_string_ostream OS(strDump);
    OS << *mod;
    OS.flush();
    std::string line;
    std::vector<std::string> modLines;
    auto modStream = std::stringstream(strDump);
    while (std::getline(modStream, line, '\n'))
    {
        modLines.push_back(line);
    }
    for (auto &f : *mod)
    {
        if (f.hasExactDefinition())
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
            std::vector<llvm::Metadata *> ElTys;
            ElTys.push_back(uType);
            for (unsigned int i = 0; i < f.getNumOperands(); i++)
            {
                ElTys.push_back(uType);
            }
            auto ElTypeArray = DBuild.getOrCreateTypeArray(ElTys);
            auto SubTy = DBuild.createSubroutineType(ElTypeArray);
            auto FContext = DFile;
            std::string funcName = f.getName();
            findLine(modLines, funcName, lineNo);
            auto SP = DBuild.createFunction(FContext, f.getName(), llvm::StringRef(), DFile, lineNo, SubTy, lineNo, llvm::DINode::FlagZero, llvm::DISubprogram::DISPFlags::SPFlagDefinition);
            f.setSubprogram(SP);
            for (auto b = f.begin(); b != f.end(); b++)
            {
                findLine(modLines, "", lineNo, true);
                for (auto it = b->begin(); it != b->end(); it++)
                {
                    if (auto inst = llvm::dyn_cast<llvm::Instruction>(it))
                    {
                        if (auto al = llvm::dyn_cast<llvm::AllocaInst>(inst))
                        {
                            std::string metaString;
                            if (it->getMetadata("ValueID") != nullptr)
                            {
                                auto MDN = it->getMetadata("ValueID");
                                auto ci = llvm::cast<llvm::ConstantInt>(llvm::cast<llvm::ConstantAsMetadata>(MDN->getOperand(0))->getValue());
                                int64_t ID = ci->getSExtValue();
                                if (ID == IDState::Artificial)
                                {
                                    auto DL = llvm::DILocation::get(SP->getContext(), lineNo, 0, SP);
                                    modLines.insert(modLines.begin() + lineNo, "");
                                    lineNo++;
                                    auto DL2 = llvm::DILocation::get(SP->getContext(), lineNo, 0, SP);
                                    auto D = DBuild.createAutoVariable(SP, "export_" + std::to_string(lineNo), DFile, lineNo, uType);
                                    DBuild.insertDeclare(al, D, DBuild.createExpression(), DL, al);
                                    inst->setDebugLoc(DL2);
                                }
                            }
                            else
                            {
                                auto DL = llvm::DILocation::get(SP->getContext(), lineNo, 0, SP);
                                al->setDebugLoc(DL);
                            }
                        }
                        else
                        {
                            auto DL = llvm::DILocation::get(SP->getContext(), lineNo, 0, SP);
                            inst->setDebugLoc(DL);
                        }
                    }
                    lineNo++;
                }
            }
            DBuild.finalizeSubprogram(SP);
        }
        if (lineNo >= modLines.size())
        {
            lineNo = 0;
        }
    }
    DBuild.finalize();
}

inline void SetFunctionAnnotation(llvm::Function *F, std::string key, int64_t value)
{
    llvm::MDNode *idNode = llvm::MDNode::get(F->getContext(), llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt64Ty(F->getContext()), (uint64_t)value)));
    F->setMetadata(key, idNode);
}

inline int64_t GetFunctionAnnotation(llvm::Function *F, std::string key)
{
    int64_t result = -1;
    if (llvm::MDNode *node = F->getMetadata(key))
    {
        auto ci = llvm::cast<llvm::ConstantInt>(llvm::cast<llvm::ConstantAsMetadata>(node->getOperand(0))->getValue());
        result = ci->getSExtValue();
    }
    return result;
}

inline uint64_t GetBlockCount(llvm::Module *M)
{
    uint64_t result = 0;
    for (auto mi = M->begin(); mi != M->end(); mi++)
    {
        for (auto &fi : *mi)
        {
            if (llvm::isa<llvm::BasicBlock>(fi))
            {
                result++;
            }
        }
    }
    return result;
}

inline void VerifyModule(llvm::Module *M)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    bool broken = verifyModule(*M, &rso);
    if (broken)
    {
        auto err = rso.str();
        spdlog::critical("Tik Module Corrupted: \n" + err);
    }
}