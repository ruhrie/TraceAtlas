#pragma once
#include <json.hpp>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <string>
#include <vector>
#include <map>
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
    /// \brief   Pointer to the current kernel function. 
    /// 
    /// This pointer contains all basic blocks of the current tik representation being built.
    llvm::Function *KernelFunction = NULL;

    /// \brief   Special LLVM map containing old instructions (from the original bitcode) as keys and new instructions as values.
    ///          The kernel function's values will be remapped according to this data structure at the end of the Kernel class.
    llvm::ValueToValueMapTy VMap;

    /// \brief   Instruction specifying the condition of the loop. When generating the condition in the Loop block at the end of MorphKernelFunction, this
    ///          global variable is used as a key in the global VMap to get the proper operand for the new branch instruction.
    llvm::Value* cond = NULL;

    /// \brief   Map containing old load and store instructions as keys and new global pointers as values.
    ///          The unaltered bitcode contains locally scoped variables that are used both in the Kernel function and the memory functions.
    ///          This structure maps those local pointers to global pointers.
    std::map < llvm::Value* , llvm::GlobalValue* > GlobalMap;
    
    /// \brief   Vector containing all instructions that don't have their parent in the basic blocks of the tik representation.
    ///          These instructions become the input arguments to the Kernel function.
    std::vector<llvm::Instruction* > ExternalValues;

    ///
    /// \brief   Function for remapping instructions based on VMap.
    ///          This is done before morphing KernelFunction into a new function with inputs.
    void Remap();

    /// \brief   Extracts the instructions necessary for maintaining the loop of KernelFunction.
    ///          These instructions are identified by looking for block terminators, and their successors in the bitcode.
    ///          The conditional exit instructions whose successors are in KernelFunction
    /// \param   blocks      Vector of basic blocks in the module passed to the constructor
    /// 
    void GetLoopInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetBodyInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetInitInsts(std::vector<llvm::BasicBlock *> blocks);
    void GetExits(std::vector<llvm::BasicBlock *> blocks);
    void CreateExitBlock(void);
    void GetMemoryFunctions(void);
    void MorphKernelFunction(std::vector<llvm::BasicBlock* > blocks);

};