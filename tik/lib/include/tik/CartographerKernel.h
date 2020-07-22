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
        CartographerKernel(std::vector<int64_t> basicBlocks, std::string name = "");

    private:
        CartographerKernel();
        void GetBoundaryValues(std::set<llvm::BasicBlock *> &blocks, std::vector<int64_t> &KernelImports);
        void BuildKernelFromBlocks(llvm::ValueToValueMapTy &VMap, std::set<llvm::BasicBlock *> &blocks);
        void InlineFunctionsFromBlocks(std::set<int64_t> &blocks);
        void RemapNestedKernels(llvm::ValueToValueMapTy &VMap);
        void CopyGlobals(llvm::ValueToValueMapTy &VMap);
        void BuildInit(llvm::ValueToValueMapTy &VMap);
        void BuildExit();
        void PatchPhis(llvm::ValueToValueMapTy &VMap);
        void Remap(llvm::ValueToValueMapTy &VMap);
        void FixInvokes();
    };
} // namespace TraceAtlas::tik