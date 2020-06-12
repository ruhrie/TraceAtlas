#include "dot.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "cartographer.h"
#include <llvm/IR/CFG.h>

using namespace std;
using namespace llvm;

string GenerateDot(const set<set<int64_t>> &kernels)
{
    map<int64_t, set<int64_t>> kernelMap;
    map<int64_t, set<int64_t>> bottomKernelMap;
    map<int64_t, set<int64_t>> parentMap;
    int k = 0;
    set<int64_t> allBlocks;
    for (const auto &kernel : kernels)
    {
        kernelMap[k++] = kernel;
        allBlocks.insert(kernel.begin(), kernel.end());
    }

    for (const auto &kernel : kernelMap)
    {
        for (const auto &kernel2 : kernelMap)
        {
            if (kernel.first == kernel2.first)
            {
                continue;
            }
            vector<int> intSet;
            set_difference(kernel.second.begin(), kernel.second.end(), kernel2.second.begin(), kernel2.second.end(), std::inserter(intSet, intSet.begin()));
            if (intSet.empty())
            {
                parentMap[kernel.first].insert(kernel2.first);
            }
        }
    }

    for (auto block : allBlocks)
    {
        vector<int64_t> kernelsPresent;
        for (auto k : kernelMap)
        {
            if (k.second.find(block) != k.second.end())
            {
                kernelsPresent.push_back(k.first);
            }
        }
        for (auto k : kernelsPresent)
        {
            bool done = false;
            if (parentMap.find(k) == parentMap.end())
            {
                bottomKernelMap[k].insert(block);
                done = true;
            }
            else
            {
                auto children = parentMap[k];
                if (children.find(k) == children.end())
                {
                    bottomKernelMap[k].insert(block);
                    done = true;
                }
            }
            if (done)
            {
                break;
            }
        }
    }

    string result = "digraph{\n";
    int j = 0;
    for (const auto &kernel : kernels)
    {
        result += "\tsubgraph cluster_" + to_string(j) + "{\n";
        result += "\t\tlabel=\"Kernel " + to_string(j++) + "\";\n";
        for (auto b : kernel)
        {
            result += "\t\t" + to_string(b) + ";\n";
        }
        result += "\t}\n";
    }
    for (auto b : allBlocks)
    {
        auto block = blockMap[b];
        for (auto suc : successors(block))
        {
            auto sucBlock = GetBlockID(suc);
            if (allBlocks.find(sucBlock) != allBlocks.end())
            {
                result += "\t" + to_string(b) + " -> " + to_string(sucBlock) + ";\n";
            }
        }
        for (auto bi = block->begin(); bi != block->end(); bi++)
        {
            if (auto ci = dyn_cast<CallInst>(bi))
            {
                auto F = ci->getCalledFunction();
                if (F != nullptr && !F->empty())
                {
                    BasicBlock *entry = &F->getEntryBlock();
                    auto id = GetBlockID(entry);
                    result += "\t" + to_string(b) + " -> " + to_string(id) + " [style=dashed];\n";
                }
            }
        }
    }
    result += "}";

    return result;
}