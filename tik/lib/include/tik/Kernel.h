#pragma once
#include "tik/KernelInterface.h"
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace TraceAtlas::tik
{
    extern int KernelUID;
    extern std::set<std::string> reservedNames;
    class Kernel
    {
    public:
        ~Kernel();
        std::string GetHeaderDeclaration(std::set<llvm::StructType *> &AllStructures);
        std::string Name;
        std::set<llvm::BasicBlock *> Conditional;

        std::set<std::shared_ptr<KernelInterface>> Entrances;

        std::set<std::shared_ptr<KernelInterface>> Exits;

        llvm::BasicBlock *Init = nullptr;
        llvm::BasicBlock *Exit = nullptr;
        llvm::BasicBlock *Exception = nullptr;
        llvm::Function *KernelFunction = nullptr;
        bool Valid = false;
        std::map<llvm::Argument *, int64_t> ArgumentMap;

    protected:
        Kernel();
        void ApplyMetadata(std::map<llvm::Value *, llvm::GlobalObject *> &GlobalMap);

    private:
        Kernel(const Kernel &) = delete;
    };
} // namespace TraceAtlas::tik
