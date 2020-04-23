#pragma once
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

extern int KernelUID;
extern std::set<std::string> reservedNames;
class Kernel
{
public:
    ~Kernel();
    std::string GetHeaderDeclaration(std::set<llvm::StructType *> &AllStructures);
    std::string Name;
    std::set<llvm::BasicBlock *> Conditional;

    /// @brief  Must be a member because we may dereference it from the KernelMap when building a kernel
    std::set<llvm::BasicBlock *> Entrances;

    /// @brief  Must be a member because we may dereference it when building a kernel with an embedded call
    std::map<int, llvm::BasicBlock *> ExitTarget;
    llvm::BasicBlock *Init = nullptr;
    llvm::BasicBlock *Exit = nullptr;
    llvm::BasicBlock *Exception = nullptr;
    llvm::Function *KernelFunction = nullptr;
    bool Valid = false;
    std::vector<llvm::Value *> KernelImports;
    std::vector<llvm::Value *> KernelExports;
    std::map<llvm::BasicBlock *, int> ExitMap;
    std::map<llvm::Argument *, llvm::Value *> ArgumentMap;
    std::map<llvm::BasicBlock *, llvm::BasicBlock *> ExitBlockMap;
protected:
    Kernel();
    void ApplyMetadata(std::map<llvm::Value *, llvm::GlobalObject *> &GlobalMap);
private:
    ///@brief   Must be a member because we may dereference it when building a kernel with an embedded call
    std::map<int, llvm::GlobalValue *> LoadMap;
    std::map<int, llvm::GlobalValue *> StoreMap;
};
