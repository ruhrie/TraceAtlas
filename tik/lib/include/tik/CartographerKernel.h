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

        /// @brief Finds all imports and exports of the kernel
        ///
        /// Imports are instructions from outside the kernel blocks whose value is used within the kernel.
        /// Exports are instructions that exist within the kernel, whose value is used outside the kernel.
        /// After this function, the kernel function signature is made, where the imports have the type of their underlying values, and the exports are pointers to their underlying values.
        /// @param scopedBlocks    Set of basic blocks that include all kernel blocks as well as blocks of embedded function calls
        /// @param scopedFuncs     Set of functions that exist within scopedBlocks
        /// @param embeddedKernels Set of functions that are embedded kernel calls
        /// @param KernelImports   Value IDs that represent all imports of the kernel
        /// @param KernelExports   Value IDs representing all kernel exports.
        void GetBoundaryValues(std::set<llvm::BasicBlock *> &scopedBlocks, std::set<llvm::Function *> &scopedFuncs, std::set<llvm::Function *> &embeddedKernels, std::vector<int64_t> &KernelImports, std::vector<int64_t> &KernelExports, llvm::ValueToValueMapTy &VMap);

        /// @brief Constructs kernel function from only kernel blocks
        ///
        /// Uses the block set from cartographer input to construct entire kernel function signature, where function calls are not yet inlined.
        /// Creates embedded calls for all embedded kernels, and remaps their arguments to either parent function args or in-context allocs.
        void BuildKernelFromBlocks(llvm::ValueToValueMapTy &VMap, std::set<llvm::BasicBlock *> &blocks);

        /// @brief Constructs basic block that allocs for embedded exports and handles entrance vector
        ///
        /// When embedded kernels export to the parent, their pointers are allocated here.
        /// The entrance of the kernel is handled here.
        void BuildInit(llvm::ValueToValueMapTy &VMap, std::vector<int64_t> &KernelExports);

        /// @brief Inlines all functions that can possibly be inlined
        ///
        /// Only functions that are internally defined are inlined.
        /// When the original bitcode is compiled, some functions (from libc and STL, for example) are external references.
        void InlineFunctionsFromBlocks(std::set<int64_t> &blocks);

        /// Not used anymore
        void RemapNestedKernels(llvm::ValueToValueMapTy &VMap);

        /// @brief Moves all global definitions internal to the source bitcode into the tik module
        ///
        /// Certain values like Operators and Global Values cannot have their operands changed through remapping.
        /// It is required to copy all global references into the tik module, and change those embedded operators to use the local definitions.
        void CopyGlobals(llvm::ValueToValueMapTy &VMap);

        /// @brief Constructs exit block(s)
        ///
        /// Each valid exit of the kernel gets its own block that branches to the true exit.
        /// The successor of each of these blocks returns the correct exit integer based on who its predecessor was.
        void BuildExit();

        /// @brief Removes all pairs from phi nodes whose incoming block is not a valid predecessor.
        ///
        /// Invalid predecessors are blocks who either do not belong to the kernel function or are not valid predecessors of the phi node parent.
        void PatchPhis(llvm::ValueToValueMapTy &VMap);

        /// @brief Remaps operands manually and remaps all entries in VMap
        ///
        /// After this function completes, all entries in the VMap are invalidated.
        /// This function must be called before and after inlining.
        void Remap(llvm::ValueToValueMapTy &VMap);

        /// @brief Attempts to replicate exception handling from the original bitcode.
        void FixInvokes();
    };
} // namespace TraceAtlas::tik