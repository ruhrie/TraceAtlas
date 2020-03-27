#include "TypeTwo.h"
#include "cartographer.h"
#include <set>
#include <string>

using namespace std;
using namespace llvm;

namespace TypeTwo
{
    uint64_t blockCount = 0;

    int *openCount = NULL;
    set<int64_t> *finalBlocks = NULL;
    set<int> *kernelMap = NULL;
    set<int64_t> openBlocks;
    int *kernelStarts = NULL;
    set<int64_t> *blocks = NULL;

    bool blocksLabeled = false;
    string currentKernel;
    std::set<std::set<int64_t>> kernels;
    void Setup(llvm::Module *bitcode, std::set<std::set<int64_t>> k)
    {
        for (auto mi = bitcode->begin(); mi != bitcode->end(); mi++)
        {
            for (auto fi = mi->begin(); fi != mi->end(); fi++)
            {
                blockCount++;
            }
        }

        kernels = k;

        openCount = (int *)calloc(sizeof(int), blockCount);                         // counter to know where we are in the callstack
        finalBlocks = (set<int64_t> *)calloc(sizeof(set<int64_t>), kernels.size()); // final kernel definitions
        kernelStarts = (int *)calloc(sizeof(int), kernels.size());                  // map of a kernel index to the first block seen
        blocks = (set<int64_t> *)calloc(sizeof(set<int64_t>), kernels.size());      // temporary kernel blocks
        kernelMap = (set<int> *)calloc(sizeof(set<int>), blockCount);
        for (uint32_t i = 0; i < blockCount; i++)
        {
            kernelMap[i] = set<int>();
            openCount[i] = 0;
        }
        for (uint32_t i = 0; i < kernels.size(); i++)
        {
            blocks[i] = set<int64_t>();
            finalBlocks[i] = set<int64_t>();
            kernelStarts[i] = -1;
        }
        int a = 0;
        for (auto kernel : kernels)
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
            int block = stoi(value, 0, 0);
            openCount[block]++; //mark this block as being entered
            openBlocks.insert(block);

            for (uint64_t i = 0; i < kernels.size(); i++)
            {
                blocks[i].insert(block);
            }
            if (!blocksLabeled && !currentKernel.empty())
            {
                blockLabelMap[block].insert(currentKernel);
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
            int block = stoi(value, 0, 0);
            openCount[block]--;
            if (openCount[block] == 0)
            {
                openBlocks.erase(block);
            }
        }
        else if (key == "KernelEnter")
        {
            currentKernel = value;
        }
        else if (key == "KernelExit")
        {
            currentKernel.clear();
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