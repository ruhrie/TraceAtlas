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
