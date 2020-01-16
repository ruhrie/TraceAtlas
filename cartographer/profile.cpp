#include "profile.h"
#include "EncodeDetect.h"
#include "AtlasUtil/Annotate.h"
#include <iostream>

using namespace std;
using namespace llvm;

map<int, map<string, int>> rMap;

map<int, map<string, int>> ProfileKernels(std::map<int, std::set<int>> kernels, Module *M)
{
    //annotate it with the same algorithm used in the tracer
    Annotate(M);
    //start by profiling every basic block
    for (Module::iterator F = M->begin(), E = M->end(); F != E; ++F)
    {
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            ProfileBlock(cast<BasicBlock>(BB));
        }
    }

    map<int, map<string, int>> result;

    for(auto kernel : kernels)
    {
        int index = kernel.first;
        auto blocks = kernel.second;
        for(auto block : blocks)
        {
            int count = blockCount[block];
            for(auto pair : rMap[block])
            {
                result[index][pair.first] += pair.second * count;
            }
        }
    }
    
    return result;
}

void ProfileBlock(BasicBlock *BB)
{
    string blockName = BB->getName();
    uint64_t id = std::stoul(blockName.substr(7));
    for (auto bi = BB->begin(); bi != BB->end(); bi++)
    {
        Instruction *i = cast<Instruction>(bi);
        if (i->getMetadata("TikSynthetic"))
        {
            continue;
        }
        //start with the opcodes
        string name = string(i->getOpcodeName());
        rMap[id][name + "Count"]++;
        //now check the type
        Type *t = i->getType();
        if (t->isVoidTy())
        {
            rMap[id]["typeVoid"]++;
        }
        else if (t->isFloatingPointTy())
        {
            rMap[id]["typeFloat"]++;
        }
        else if (t->isIntegerTy())
        {
            rMap[id]["typeInt"]++;
        }
        else if (t->isArrayTy())
        {
            rMap[id]["typeArray"]++;
        }
        else if (t->isVectorTy())
        {
            rMap[id]["typeVector"]++;
        }
        else if (t->isPointerTy())
        {
            rMap[id]["typePointer"]++;
        }
        else
        {
            std::string str;
            llvm::raw_string_ostream rso(str);
            t->print(rso);
            cerr << "Unrecognized type: " + str + "\n";
        }
        rMap[id]["instructionCount"]++;
    }
}