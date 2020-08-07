#include "profile.h"
#include "AtlasUtil/Format.h"
#include "TypeOne.h"
#include <iostream>
#include <llvm/Support/raw_ostream.h>

using namespace std;
using namespace llvm;

map<string, map<string, map<string, int>>> ProfileKernels(const std::map<int, std::set<int64_t>> &kernels, Module *M)
{
    map<int64_t, map<string, uint64_t>> rMap;  //dictionary which keeps track of the actual information per block
    map<int64_t, map<string, uint64_t>> cpMap; //dictionary which keeps track of the cross product information per block
    //annotate it with the same algorithm used in the tracer
    Format(M);
    //start by profiling every basic block
    for (auto &F : *M)
    {
        for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
        {
            ProfileBlock(cast<BasicBlock>(BB), rMap, cpMap);
        }
    }

    map<string, map<string, map<string, int>>> fin;

    map<string, map<string, int>> cPigData;  //from the trace
    map<string, map<string, int>> pigData;   //from the bitcode
    map<string, map<string, int>> ecPigData; //cross product from the trace
    map<string, map<string, int>> epigData;  //cross product from the bitcode

    for (const auto &kernel : kernels)
    {
        int index = kernel.first;
        string iString = to_string(index);
        auto blocks = kernel.second;
        for (auto block : blocks)
        {
            uint64_t count = TypeOne::blockCount[block];
            for (const auto &pair : rMap[block])
            {
                cPigData[iString][pair.first] += pair.second * count;
                pigData[iString][pair.first] += pair.second;
            }

            for (const auto &pair : cpMap[block])
            {
                ecPigData[iString][pair.first] += pair.second * count;
                epigData[iString][pair.first] += pair.second;
            }
        }
    }

    fin["CPig"] = cPigData;
    fin["Pig"] = pigData;
    fin["ECPig"] = ecPigData;
    fin["EPig"] = epigData;
    return fin;
}

void ProfileBlock(BasicBlock *BB, map<int64_t, map<string, uint64_t>> &rMap, map<int64_t, map<string, uint64_t>> &cpMap)
{
    int64_t id = GetBlockID(BB);
    for (auto bi = BB->begin(); bi != BB->end(); bi++)
    {
        auto *i = cast<Instruction>(bi);
        if (i->getMetadata("TikSynthetic") != nullptr)
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
            cpMap[id][name + "typeVoid"]++;
        }
        else if (t->isFloatingPointTy())
        {
            rMap[id]["typeFloat"]++;
            cpMap[id][name + "typeFloat"]++;
        }
        else if (t->isIntegerTy())
        {
            rMap[id]["typeInt"]++;
            cpMap[id][name + "typeInt"]++;
        }
        else if (t->isArrayTy())
        {
            rMap[id]["typeArray"]++;
            cpMap[id][name + "typeArray"]++;
        }
        else if (t->isVectorTy())
        {
            rMap[id]["typeVector"]++;
            cpMap[id][name + "typeVector"]++;
        }
        else if (t->isPointerTy())
        {
            rMap[id]["typePointer"]++;
            cpMap[id][name + "typePointer"]++;
        }
        else
        {
            std::string str;
            llvm::raw_string_ostream rso(str);
            t->print(rso);
            cerr << "Unrecognized type: " + str + "\n";
        }
        rMap[id]["instructionCount"]++;
        cpMap[id]["instructionCount"]++;
    }
}
