#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "tik/CartographerKernel.h"
#include "tik/Header.h"
#include "tik/Metadata.h"
#include "tik/Util.h"
#include "tik/libtik.h"
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

namespace TraceAtlas::tik
{
    int KernelUID = 0;

    set<string> reservedNames;

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

    void Kernel::ApplyMetadata(std::map<llvm::Value *, llvm::GlobalObject *> &GlobalMap)
    {
        //first we will clean the current instructions
        for (auto &fi : *KernelFunction)
        {
            for (auto bi = fi.begin(); bi != fi.end(); bi++)
            {
                auto inst = cast<Instruction>(bi);
                inst->setMetadata("dbg", nullptr);
            }
        }
        //second remove all debug intrinsics
        vector<Instruction *> toRemove;
        for (auto &fi : *KernelFunction)
        {
            for (auto bi = fi.begin(); bi != fi.end(); bi++)
            {
                if (auto di = dyn_cast<DbgInfoIntrinsic>(bi))
                {
                    toRemove.push_back(di);
                }
                auto inst = cast<Instruction>(bi);
                inst->setMetadata("dbg", nullptr);
            }
        }
        for (auto r : toRemove)
        {
            r->eraseFromParent();
        }
        //annotate the kernel functions
        string metadata = "{\n\t\"Entrances\": {\n\t\t\"Blocks\": [";
        for (auto &index : Entrances)
        {
            if (index != *(Entrances.begin()))
            {
                metadata += ", ";
            }
            metadata += to_string(index->Block);
        }
        metadata += "],\n\t\t\"Indices\": [";
        for (auto &index : Entrances)
        {
            if (index != *(Entrances.begin()))
            {
                metadata += ", ";
            }
            metadata += to_string(index->Index);
        }
        metadata += "]\n\t},\n\t\"Exits\": {\n\t\t\"Blocks\": [";
        for (auto &index : Exits)
        {
            if (index != *(Exits.begin()))
            {
                metadata += ", ";
            }
            metadata += to_string(index->Block);
        }
        metadata += "],\n\t\t\"Indices\": [";
        for (auto &index : Exits)
        {
            if (index != *(Exits.begin()))
            {
                metadata += ", ";
            }
            metadata += to_string(index->Index);
        }
        metadata += "]\n\t},\n\t\"Arguments\": [";
        for (auto arg = KernelFunction->arg_begin(); arg != KernelFunction->arg_end(); arg++)
        {
            if (arg != KernelFunction->arg_begin())
            {
                metadata += ", ";
            }
            auto argVal = ArgumentMap[arg];
            metadata += to_string(argVal);
        }
        metadata += "]\n}";
        MDNode *kernelNode = MDNode::get(TikModule->getContext(), MDString::get(TikModule->getContext(), Name));
        KernelFunction->setMetadata("KernelName", kernelNode);
        MDNode *json = MDNode::get(TikModule->getContext(), MDString::get(TikModule->getContext(), metadata));
        KernelFunction->setMetadata("Boundaries", json);
        int i = 0;
        for (auto &ent : Entrances)
        {
            MDNode *newNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t) static_cast<int>(ent->Index))));
            KernelFunction->setMetadata("Ent" + to_string(i), newNode);
            i++;
        }
        i = 0;
        for (auto &ex : Exits)
        {
            MDNode *newNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t) static_cast<int>(ex->Index))));
            KernelFunction->setMetadata("Ex" + to_string(i), newNode);
            i++;
        }
        for (auto global : GlobalMap)
        {
            global.second->setMetadata("KernelName", kernelNode);
        }
        //annotate the conditional, has to happen after body since conditional is a part of the body
        MDNode *condNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikMetadata::Conditional))));
        for (auto cond : Conditional)
        {
            cast<Instruction>(cond->getFirstInsertionPt())->setMetadata("TikMetadata", condNode);
        }
    }
    Kernel::Kernel() = default;
} // namespace TraceAtlas::tik