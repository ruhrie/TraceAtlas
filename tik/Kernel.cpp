#include "tik/Kernel.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "tik/InlineStruct.h"
#include "tik/KernelConstruction.h"
#include "tik/Metadata.h"
#include "tik/TikHeader.h"
#include "tik/Util.h"
#include "tik/tik.h"
#include <algorithm>
#include <iostream>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Comdat.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <queue>
#include <spdlog/spdlog.h>

using namespace llvm;
using namespace std;

static int KernelUID = 0;

set<string> reservedNames;

Kernel::Kernel(std::vector<int64_t> basicBlocks, Module *M, string name)
{
    llvm::ValueToValueMapTy VMap;
    set<int64_t> blockSet;
    for (auto b : basicBlocks)
    {
        blockSet.insert(b);
    }
    string Name;
    if (name.empty())
    {
        Name = "Kernel_" + to_string(KernelUID++);
    }
    else if (name.front() >= '0' && name.front() <= '9')
    {
        Name = "K" + name;
    }
    else
    {
        Name = name;
    }
    if (reservedNames.find(Name) != reservedNames.end())
    {
        throw AtlasException("Kernel Error: Kernel names must be unique!");
    }
    spdlog::debug("Started converting kernel {0}", Name);
    reservedNames.insert(Name);

    set<BasicBlock *> blocks;
    for (auto &F : *M)
    {
        for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
        {
            auto *b = cast<BasicBlock>(BB);
            int64_t id = GetBlockID(b);
            if (id != -1)
            {
                if (find(basicBlocks.begin(), basicBlocks.end(), id) != basicBlocks.end())
                {
                    blocks.insert(b);
                }
            }
        }
    }

    try
    {
        //this is a recursion check, just so we can enumerate issues
        for (auto block : blocks)
        {
            Function *f = block->getParent();
            for (auto bi = block->begin(); bi != block->end(); bi++)
            {
                if (auto *cb = dyn_cast<CallBase>(bi))
                {
                    if (cb->getCalledFunction() == f)
                    {
                        throw AtlasException("Tik Error: Recursion is unimplemented")
                    }
                    if (isa<InvokeInst>(cb))
                    {
                        throw AtlasException("Invoke Inst is unsupported")
                    }
                }
            }
        }

        //SplitBlocks(blocks);
        std::map<llvm::Value *, llvm::GlobalObject *> GlobalMap;
        GetEntrances(blocks, Entrances);
        GetExits(blocks, ExitTarget);
        //std::vector<llvm::Value* > KernelImports;
        std::vector<llvm::Value *> KernelExports = GetExternalValues(blocks, VMap, KernelImports);

        //we now have all the information we need

        //start by making the correct function
        std::vector<llvm::Type *> inputArgs;
        inputArgs.push_back(Type::getInt8Ty(TikModule->getContext()));
        for (auto inst : KernelImports)
        {
            inputArgs.push_back(inst->getType());
        }
        for (auto inst : KernelExports)
        {
            inputArgs.push_back(PointerType::get(inst->getType(), 0));
        }
        FunctionType *funcType = FunctionType::get(Type::getInt8Ty(TikModule->getContext()), inputArgs, false);
        KernelFunction = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, Name, TikModule);
        uint64_t i;
        for (i = 0; i < KernelImports.size(); i++)
        {
            auto *a = cast<Argument>(KernelFunction->arg_begin() + 1 + i);
            a->setName("i" + to_string(i));
            VMap[KernelImports[i]] = a;
            ArgumentMap[a] = KernelImports[i];
        }
        uint64_t j;
        for (j = 0; j < KernelExports.size(); j++)
        {
            auto *a = cast<Argument>(KernelFunction->arg_begin() + 1 + i + j);
            a->setName("e" + to_string(j));
            ArgumentMap[a] = KernelExports[j];
        }

        //create the artificial blocks
        Init = BasicBlock::Create(TikModule->getContext(), "Init", KernelFunction);
        Exit = BasicBlock::Create(TikModule->getContext(), "Exit", KernelFunction);
        Exception = BasicBlock::Create(TikModule->getContext(), "Exception", KernelFunction);

        //copy the appropriate blocks
        set<llvm::BasicBlock *> Conditional;
        BuildKernel(KernelFunction, blocks, Conditional, Entrances, Exception, Exit, ExitMap, VMap, Init);

        Remap(VMap, KernelFunction); //we need to remap before inlining

        InlineFunctions(KernelFunction, blockSet, Init, Exception, Exit);

        CopyGlobals(KernelFunction, VMap);

        //remap and repipe
        Remap(VMap, KernelFunction);

        //might be fused
        Repipe(KernelFunction, Exit, ExitBlockMap);

        // replace external function calls with tik declarations
        ExportFunctionSignatures(KernelFunction);

        //handle the memory operations
        //GetMemoryFunctions();

        //UpdateMemory();

        BuildInit(KernelFunction, VMap, Init, Exception, Entrances);

        BuildExit(VMap, Exit, Exception, ExitMap);

        RemapNestedKernels(KernelFunction, VMap, ArgumentMap);

        RemapExports(KernelFunction, VMap, Init, KernelExports);

        PatchPhis(KernelFunction);

        //apply metadata
        ApplyMetadata(KernelFunction, Conditional, Name, GlobalMap);

        //and set a flag that we succeeded
        Valid = true;
    }
    catch (AtlasException &e)
    {
        spdlog::error(e.what());
        Cleanup();
    }

    try
    {
        //GetKernelLabels();
    }
    catch (AtlasException &e)
    {
        spdlog::warn("Failed to annotate Loop/Memory grammars");
        spdlog::debug(e.what());
    }
}

void Kernel::Cleanup()
{
    if (KernelFunction != nullptr)
    {
        KernelFunction->eraseFromParent();
    }
    /*
    if (MemoryRead)
    {
        MemoryRead->eraseFromParent();
    }
    if (MemoryWrite)
    {
        MemoryWrite->eraseFromParent();
    }
    for (auto g : GlobalMap)
    {
        g.second->eraseFromParent();
    }
    */
}

Kernel::~Kernel() = default;

/*
void Kernel::UpdateMemory()
{
    std::set<llvm::Value *> coveredGlobals;
    std::set<llvm::StoreInst *> newStores;
    IRBuilder<> initBuilder(Init);
    for (uint64_t i = 0; i < KernelImports.size(); i++)
    {
        if (GlobalMap.find(VMap[KernelImports[i]]) != GlobalMap.end())
        {
            if (GlobalMap[VMap[KernelImports[i]]] == nullptr)
            {
                throw AtlasException("Tik Error: External Value not found in GlobalMap.");
            }
            coveredGlobals.insert(GlobalMap[VMap[KernelImports[i]]]);
            auto b = initBuilder.CreateStore(KernelFunction->arg_begin() + i + 1, GlobalMap[VMap[KernelImports[i]]]);
            MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Store))));
            b->setMetadata("TikSynthetic", tikNode);
            newStores.insert(b);
        }
    }

    for (auto &bb : *KernelFunction)
    {
        for (BasicBlock::iterator BI = bb.begin(), BE = bb.end(); BI != BE; ++BI)
        {
            auto *inst = cast<Instruction>(BI);
            for (auto pair : GlobalMap)
            {
                if (pair.first == inst)
                {
                    if (coveredGlobals.find(pair.second) == coveredGlobals.end())
                    {
                        if (isa<InvokeInst>(inst))
                        {
                            throw AtlasException("Invoke is unsupported");
                        }
                        IRBuilder<> builder(inst->getNextNode());
                        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
                        auto a = builder.CreateGEP(inst->getType(), pair.second, constant);
                        auto b = builder.CreateStore(inst, a);
                        MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Store))));
                        b->setMetadata("TikSynthetic", tikNode);
                    }
                }
            }
        }
    }
}
*/

//if the result is one entry long it is a value. Otherwise its a list of instructions
vector<Value *> Kernel::BuildReturnTree(BasicBlock *bb, vector<BasicBlock *> blocks)
{
    vector<Value *> result;
    Instruction *term = bb->getTerminator();
    if (auto *retInst = dyn_cast<ReturnInst>(term))
    {
        //so the block is a return, just return the value
        result.push_back(retInst->getReturnValue());
    }
    else if (auto *brInst = dyn_cast<BranchInst>(term))
    {
        //we have a branch, if it is unconditional recurse on the target
        //otherwise recurse on all targets and build a select between the results
        //we assume the last entry in the vector is the result to be selected, so be careful on the order of insertion
        if (brInst->isConditional())
        {
            //we have a conditional so select between return vals
            //still need to check if successors are valid though
            Value *cond = brInst->getCondition();
            auto suc0 = brInst->getSuccessor(0);
            auto suc1 = brInst->getSuccessor(1);
            bool valid0 = find(blocks.begin(), blocks.end(), suc0) != blocks.end();
            bool valid1 = find(blocks.begin(), blocks.end(), suc1) != blocks.end();
            if (!(valid0 || valid1))
            {
                throw AtlasException("Tik Error: Branch instruction with no valid successors reached");
            }
            Value *c0 = nullptr;
            Value *c1 = nullptr;
            if (valid0) //if path 0 is valid we examine it
            {
                auto sub0 = BuildReturnTree(suc0, blocks);
                if (sub0.size() != 1)
                {
                    //we can just copy them
                    //otherwise this is a single value and should just be referenced
                    result.insert(result.end(), sub0.begin(), sub0.end());
                }
                c0 = sub0.back();
            }
            if (valid1) //if path 1 is valid we examine it
            {
                auto sub1 = BuildReturnTree(brInst->getSuccessor(1), blocks);
                if (sub1.size() != 1)
                {
                    //we can just copy them
                    //otherwise this is a single value and should just be referenced
                    result.insert(result.end(), sub1.begin(), sub1.end());
                }
                c1 = sub1.back();
            }
            if (valid0 && valid1) //if they are both valid we need to select between them
            {
                SelectInst *sInst = SelectInst::Create(cond, c0, c1);
                result.push_back(sInst);
            }
        }
        else
        {
            //we are not conditional so just recursef
            auto sub = BuildReturnTree(brInst->getSuccessor(0), blocks);
            result.insert(result.end(), sub.begin(), sub.end());
        }
    }
    else
    {
        throw AtlasException("Tik Error: Not Implemented");
    }
    if (result.empty())
    {
        throw AtlasException("Tik Error: Return instruction tree must have at least one result");
    }
    return result;
}

std::string Kernel::GetHeaderDeclaration(std::set<llvm::StructType *> &AllStructures)
{
    std::string headerString;
    try
    {
        headerString = getCType(KernelFunction->getReturnType(), AllStructures) + " ";
    }
    catch (AtlasException &e)
    {
        spdlog::error(e.what());
        headerString = "TypeNotSupported ";
    }
    headerString += KernelFunction->getName();
    headerString += "(";
    int i = 0;
    for (auto ai = KernelFunction->arg_begin(); ai < KernelFunction->arg_end(); ai++)
    {
        std::string type;
        std::string argname = "arg" + std::to_string(i);
        if (i > 0)
        {
            headerString += ", ";
        }
        try
        {
            type = getCType(ai->getType(), AllStructures);
        }
        catch (AtlasException &e)
        {
            spdlog::error(e.what());
            type = "TypeNotSupported";
        }
        if (type.find('!') != std::string::npos)
        {
            ProcessArrayArgument(type, argname);
        }
        else if (type.find('@') != std::string::npos)
        {
            ProcessFunctionArgument(type, argname);
        }
        else
        {
            type += " " + argname;
        }
        headerString += type;
        i++;
    }
    headerString += ");\n";
    return headerString;
}
