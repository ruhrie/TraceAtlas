#pragma once
#include "tik/Kernel.h"
#include <llvm/IR/Module.h>
#include <set>
#include <string>
#include <vector>

namespace TraceAtlas::tik
{
    class CartographerKernel : public Kernel
    {
    public:
        CartographerKernel(std::vector<int64_t> basicBlocks, llvm::Module *M, std::string name = "");
        CartographerKernel(llvm::Function *kernFunc);
        std::vector<llvm::Value *> KernelImports;
        std::vector<llvm::Value *> KernelExports;

    private:
        CartographerKernel();
        void GetBoundaryValues(std::set<llvm::BasicBlock *> &blocks);
        void BuildKernelFromBlocks(llvm::ValueToValueMapTy &VMap, std::set<llvm::BasicBlock *> &blocks);
        void InlineFunctionsFromBlocks(std::set<int64_t> &blocks);
        void RemapNestedKernels(llvm::ValueToValueMapTy &VMap, std::map<int64_t, llvm::Value*>& ArgumentValueMap);
        void RemapExports(llvm::ValueToValueMapTy &VMap);
        void CopyGlobals(llvm::ValueToValueMapTy &VMap);
        void BuildInit(llvm::ValueToValueMapTy &VMap);
        void BuildExit();
        void PatchPhis();
        void Remap(llvm::ValueToValueMapTy &VMap);
    };
} // namespace TraceAtlas::tik