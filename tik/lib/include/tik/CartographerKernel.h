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

    private:
        CartographerKernel();
        void GetBoundaryValues(std::set<llvm::BasicBlock *> &blocks, std::vector<int64_t> &KernelImports, std::vector<int64_t> &KernelExports);
        void BuildKernelFromBlocks(llvm::ValueToValueMapTy &VMap, std::set<llvm::BasicBlock *> &blocks);
        void InlineFunctionsFromBlocks(std::set<int64_t> &blocks);
        void RemapNestedKernels(llvm::ValueToValueMapTy &VMap);
        void RemapExports(llvm::ValueToValueMapTy &VMap, std::vector<int64_t> &KernelExports);
        void CopyGlobals(llvm::ValueToValueMapTy &VMap);
        void BuildInit(llvm::ValueToValueMapTy &VMap);
        void BuildExit();
        void PatchPhis();
        void Remap(llvm::ValueToValueMapTy &VMap);
    };
} // namespace TraceAtlas::tik