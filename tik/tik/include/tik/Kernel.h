#pragma once
#include "tik/InlineStruct.h"
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <tuple>
#include <vector>
class Kernel
{
public:
    Kernel(std::vector<int> basicBlocks, llvm::Module *M, std::string name = "");
    ~Kernel();
    std::string GetHeaderDeclaration(std::set<llvm::StructType *> &AllStructures);
    std::string Name;
    nlohmann::json GetJson();
    std::set<llvm::BasicBlock *> Conditional;
    std::set<llvm::BasicBlock *> Entrances;
    std::map<int, llvm::BasicBlock *> ExitTarget;
    std::set<llvm::BasicBlock *> Body;
    std::set<llvm::BasicBlock *> Termination;
    llvm::BasicBlock *Init = NULL;
    llvm::BasicBlock *Exit = NULL;
    llvm::BasicBlock *Exception = NULL;
    llvm::Function *MemoryRead = NULL;
    llvm::Function *MemoryWrite = NULL;
    llvm::Function *KernelFunction = NULL;
    bool Valid = false;

private:
    void Cleanup();
    void GetEntrances(std::set<llvm::BasicBlock *> &);
    void GetExits(std::set<llvm::BasicBlock *> &);
    std::map<llvm::BasicBlock *, int> ExitMap;
    std::map<int, llvm::GlobalValue *> LoadMap;
    std::map<int, llvm::GlobalValue *> StoreMap;
    /// @brief  Maps old instructions to new instructions.
    ///
    /// Special LLVM map containing old instructions (from the original bitcode) as keys and new instructions as values.
    /// The kernel function's values will be remapped according to this data structure at the end of the Kernel class.
    llvm::ValueToValueMapTy VMap;

    /// @brief  Map containing old load and store instructions as keys and new global pointers as values.
    ///
    /// The unaltered bitcode contains locally scoped variables that are used both in the Kernel function and the memory functions.
    /// This structure maps those local pointers to global pointers.
    std::map<llvm::Value *, llvm::GlobalObject *> GlobalMap;

    ///
    ///
    std::map<llvm::Argument *, llvm::Value *> ArgumentMap;

    /// @brief  Vector containing all instructions that don't have their parent in the basic blocks of the tik representation.
    ///         These instructions become the input arguments to the Kernel function.
    std::vector<llvm::Value *> ExternalValues;

    /// @brief   Function for remapping instructions based on VMap.
    ///          This is done before morphing KernelFunction into a new function with inputs.
    void Remap();

    /// @brief  Searches for the loop condition instruction, and adds its eligible users to the body block
    ///
    /// The loop condition instruction needs to be identified and store in LoopCondition for later use in MorphKernelFunction
    /// Later, the condition's user instructions are evaluated, and those that are eligible to be in the tik representation are cloned into the VMap.
    /// Eligible instructions are those that belong to the kernel's original basic blocks, and not eligible otherwise.
    /// This function assumes that we will only find one conditional exit instruction, because we assume that the kernel will not have embedded loops in it.
    void GetConditional(std::set<llvm::BasicBlock *> &blocks);

    /// @brief  Find all instructions not initialized in the kernel representation.
    ///
    /// The parent block of each instruction in Kernel::Body is checked for its membership in the tik representation.
    /// If it is not found, that instruction is added to Kernel::Init
    void GetExternalValues(std::set<llvm::BasicBlock *> &blocks);

    /// @brief  Defines Kernel::MemoryRead and Kernel::MemoryWrite.
    ///         Defines GlobalMap.
    ///
    /// This function detects all load and store instructions in Kernel::Body and Kernel::Conditional and maps them to the functions MemoryRead (loads) and MemoryWrite (stores).
    /// Since we use functions to access memory, the pointers they use must be globally scoped so both the kernel function and memory functions can use them.
    /// GlobalMap maps relative pointers from load and store instructions to globally defined pointers.
    /// Every time we use one of these pointers in the body, we have to store to its global pointer.
    /// Finally, we find all instructions not belonging to the parent block and remove them.
    void GetMemoryFunctions();

    /// @brief  Creates a new kernel function with appropriate input args, assigns inputs to global pointers, and remaps the new function.
    ///
    /// This function replaces Kernel::KernelFunction with a new function that has the input args discovered by Kernel::getInitInsts.
    /// Then it assigns these input args to the appropriate global pointers.
    /// Finally, it remaps the new function, and the tik representation is done.
    void RemapNestedKernels();

    std::vector<llvm::Value *> BuildReturnTree(llvm::BasicBlock *bb, std::vector<llvm::BasicBlock *> blocks);

    llvm::BasicBlock *getPathMerge(llvm::BasicBlock *start);

    void ApplyMetadata();

    std::vector<InlineStruct> InlinedFunctions;

    void BuildKernel(std::set<llvm::BasicBlock *> &blocks);
    void BuildInit();
    void BuildExit();

    void UpdateMemory();

    void Repipe();
    void SplitBlocks(std::set<llvm::BasicBlock *> &blocks);
    void ExportFunctionSignatures();
    void SanityChecks();

    void CopyGlobals();

    void GetKernelLabels();
    void CopyArgument(llvm::CallBase *Call);
    void CopyOperand(llvm::User *inst);
    void InlineFunctions();
};
