#include "TypeTwo.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "cartographer.h"
#include <set>
#include <string>

using namespace std;
using namespace llvm;

namespace TypeTwo
{
    int32_t openIndicator = -1;

    int *openCount = nullptr;
    set<int64_t> *finalBlocks = nullptr;
    set<int> *kernelMap = nullptr;
    set<int64_t> openBlocks;
    int *kernelStarts = nullptr;
    set<int64_t> *blocks = nullptr;

    std::set<int32_t> *blockCaller = nullptr;

    bool blocksLabeled = false;
    vector<string> currentKernel;
    std::set<std::set<int64_t>> kernels;
    void Setup(llvm::Module *bitcode, std::set<std::set<int64_t>> k)
    {
        int64_t maxBlockId = 0;
        for (auto &mi : *bitcode)
        {
            for (auto fi = mi.begin(); fi != mi.end(); fi++)
            {
                auto b = cast<BasicBlock>(fi);
                auto id = GetBlockID(b);
                maxBlockId = max(id, maxBlockId);
            }
        }

        kernels = move(k);

        openCount = (int *)calloc(sizeof(int), maxBlockId);                         // counter to know where we are in the callstack
        finalBlocks = (set<int64_t> *)calloc(sizeof(set<int64_t>), kernels.size()); // final kernel definitions
        kernelStarts = (int *)calloc(sizeof(int), kernels.size());                  // map of a kernel index to the first block seen
        blocks = (set<int64_t> *)calloc(sizeof(set<int64_t>), kernels.size());      // temporary kernel blocks
        kernelMap = (set<int> *)calloc(sizeof(set<int>), maxBlockId);
        blockCaller = (set<int32_t> *)calloc(sizeof(set<int32_t>), maxBlockId);
        for (uint32_t i = 0; i < maxBlockId; i++)
        {
            kernelMap[i] = set<int>();
            openCount[i] = 0;
            blockCaller[i] = set<int32_t>();
        }
        for (uint32_t i = 0; i < kernels.size(); i++)
        {
            blocks[i] = set<int64_t>();
            finalBlocks[i] = set<int64_t>();
            kernelStarts[i] = -1;
        }
        int a = 0;
        for (const auto &kernel : kernels)
        {
            for (auto block : kernel)
            {
                kernelMap[block].insert(a);
            }
            a++;
        }
    }
    void Process(std::string &key, std::string &value)
    {
        if (key == "BBEnter")
        {
            int32_t block = stoi(value, nullptr, 0);
            openCount[block]++; //mark this block as being entered
            openBlocks.insert(block);

            if (openIndicator != -1)
            {
                blockCaller[openIndicator].insert(block);
            }
            openIndicator = block;

            for (uint64_t i = 0; i < kernels.size(); i++)
            {
                blocks[i].insert(block);
            }
            if (!blocksLabeled && !currentKernel.empty())
            {
                for (const auto &k : currentKernel)
                {
                    blockLabelMap[block].insert(k);
                }
            }

            for (auto open : openBlocks)
            {
                for (auto ki : kernelMap[open])
                {
                    finalBlocks[ki].insert(block);
                }
            }

            for (auto ki : kernelMap[block])
            {

                if (kernelStarts[ki] == -1)
                {
                    kernelStarts[ki] = block;
                    finalBlocks[ki].insert(block);
                }
                if (kernelStarts[ki] != block)
                {
                    finalBlocks[ki].insert(blocks[ki].begin(), blocks[ki].end());
                }
                blocks[ki].clear();
            }
        }
        else if (key == "BBExit")
        {
            int block = stoi(value, nullptr, 0);
            openCount[block]--;
            if (openCount[block] == 0)
            {
                openBlocks.erase(block);
            }
            openIndicator = -1;
        }
        else if (key == "KernelEnter")
        {
            currentKernel.push_back(value);
        }
        else if (key == "KernelExit")
        {
            if (currentKernel.back() != value)
            {
                throw AtlasException("Kernel Entrance/Exit not Matched");
            }
            currentKernel.pop_back();
        }
    }

    std::set<std::set<int64_t>> Get()
    {
        if (!blocksLabeled)
        {
            blocksLabeled = true;
        }
        std::set<set<int64_t>> finalSets;
        for (uint64_t i = 0; i < kernels.size(); i++)
        {
            finalSets.insert(finalBlocks[i]);
        }
        free(openCount);
        free(finalBlocks);
        free(kernelStarts);
        free(blocks);
        free(kernelMap);
        openBlocks.clear();
        currentKernel.clear();
        return finalSets;
    }
} // namespace TypeTwo