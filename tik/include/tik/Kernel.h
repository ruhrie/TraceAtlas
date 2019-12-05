#pragma once
#include <json.hpp>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <map>
#include <string>
#include <vector>
class Kernel
{
public:
    Kernel(std::vector<int> basicBlocks, llvm::Module *M);
    ~Kernel();
    std::string Name;
    nlohmann::json GetJson();
    llvm::BasicBlock *Conditional = NULL;
    llvm::BasicBlock *ExitTarget = NULL;
    llvm::BasicBlock *Body = NULL;
    llvm::BasicBlock *Init = NULL;
    llvm::BasicBlock *Exit = NULL;
    llvm::Function *MemoryRead = NULL;
    llvm::Function *MemoryWrite = NULL;

private:
    /// @brief  Pointer to the current kernel function.
    ///
    /// This pointer contains all basic blocks of the current tik representation being built.
    llvm::Function *KernelFunction = NULL;

    /// @brief  Maps old instructions to new instructions.
    ///
    /// Special LLVM map containing old instructions (from the original bitcode) as keys and new instructions as values.
    /// The kernel function's values will be remapped according to this data structure at the end of the Kernel class.
    llvm::ValueToValueMapTy VMap;

    /// @brief  Instruction specifying the condition of the loop.
    ///
    /// When generating the condition in the Loop block at the end of MorphKernelFunction, this global variable is used as a key in the global VMap to get the proper operand for the new branch instruction.
    llvm::Value *LoopCondition = NULL;

    /// @brief  Map containing old load and store instructions as keys and new global pointers as values.
    ///
    /// The unaltered bitcode contains locally scoped variables that are used both in the Kernel function and the memory functions.
    /// This structure maps those local pointers to global pointers.
    std::map<llvm::Value *, llvm::GlobalValue *> GlobalMap;

    /// @brief  Vector containing all instructions that don't have their parent in the basic blocks of the tik representation.
    ///         These instructions become the input arguments to the Kernel function.
    std::vector<llvm::Instruction *> ExternalValues;

    /// @brief   Function for remapping instructions based on VMap.
    ///          This is done before morphing KernelFunction into a new function with inputs.
    void Remap();

    /// @brief  Searches for the loop condition instruction, and adds its eligible users to the body block
    ///
    /// The loop condition instruction needs to be identified and store in LoopCondition for later use in MorphKernelFunction
    /// Later, the condition's user instructions are evaluated, and those that are eligible to be in the tik representation are cloned into the VMap.
    /// Eligible instructions are those that belong to the kernel's original basic blocks, and not eligible otherwise.
    /// This function assumes that we will only find one conditional exit instruction, because we assume that the kernel will not have embedded loops in it.
    ///
    /// @param   blocks     Vector of basic blocks in the module passed to the constructor.
    void GetLoopInsts(std::vector<llvm::BasicBlock *> blocks);

    /// @brief  Extracts instructions that will make up the core computations of the kernel.
    ///
    /// The function finds our first block, or entrance block, of the kernel.
    /// Then it searches the instruction path of that block for any function calls, and creates representations of those calls in the tik module.
    /// This code, as of this version, does not support internal loops.
    ///
    /// @param  blocks      Vector of basic blocks in the module passed to the constructor.
    void GetBodyInsts(std::vector<llvm::BasicBlock *> blocks);

    /// @brief  Find all instructions not initialized in the kernel representation.
    ///
    /// The parent block of each instruction in Kernel::Body is checked for its membership in the tik representation.
    /// If it is not found, that instruction is added to Kernel::Init
    ///
    /// @param  blocks      Vector of basic blocks in the module passed to the constructor.
    void GetInitInsts(std::vector<llvm::BasicBlock *> blocks);

    /// @brief  Looks through each block in blocks and checks their successors.
    ///
    /// This function checks if there is one and exactly one successor the condition at the end of the tik representation.
    /// This is because we assume that there are no embedded loops in the kernel code.
    /// Finally it assigns the one successor to Kernel::ExitTarget.
    ///
    /// @param  blocks      Vector of basic blocks in the module passed to the constructor.
    void GetExits(std::vector<llvm::BasicBlock *> blocks);

    /// @brief  Simply creates a basic block with a return instruction.
    ///         Used at the end of the tik representation.
    void CreateExitBlock(void);

    /// @brief  Defines Kernel::MemoryRead and Kernel::MemoryWrite.
    ///         Defines GlobalMap.
    ///
    /// This function detects all load and store instructions in Kernel::Body and Kernel::Conditional and maps them to the functions MemoryRead (loads) and MemoryWrite (stores).
    /// Since we use functions to access memory, the pointers they use must be globally scoped so both the kernel function and memory functions can use them.
    /// GlobalMap maps relative pointers from load and store instructions to globally defined pointers.
    /// Every time we use one of these pointers in the body, we have to store to its global pointer.
    /// Finally, we find all instructions not belonging to the parent block and remove them.
    void GetMemoryFunctions(void);

    /// @brief  Creates a new kernel function with appropriate input args, assigns inputs to global pointers, and remaps the new function.
    ///
    /// This function replaces Kernel::KernelFunction with a new function that has the input args discovered by Kernel::getInitInsts.
    /// Then it assigns these input args to the appropriate global pointers.
    /// Finally, it remaps the new function, and the tik representation is done.
    ///
    /// @param  blocks      Vector of basic blocks in the module passed to the constructor.
    void MorphKernelFunction(std::vector<llvm::BasicBlock *> blocks);

    void HandleReturnInstructions(std::vector<llvm::BasicBlock *> blocks, llvm::Function *F);

    std::vector<llvm::Instruction *> getInstructionPath(llvm::BasicBlock *start, std::vector<llvm::BasicBlock *> validBlocks);
    llvm::BasicBlock *getPathMerge(llvm::BasicBlock *start);
    std::vector<llvm::Instruction *> GetPathInstructions(llvm::BasicBlock *start, llvm::BasicBlock *end);
};