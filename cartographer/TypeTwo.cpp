#include "TypeTwo.h"
#include "AtlasUtil/Exceptions.h"
#include "cartographer.h"
#include <set>
#include <string>
#include <thread>

using namespace std;
using namespace llvm;

namespace TypeTwo
{
    thread_local int *openCount = nullptr;
    thread_local set<int64_t> *finalBlocks = nullptr;
    thread_local set<int> *kernelMap = nullptr;
    thread_local set<int64_t> openBlocks;
    thread_local int *kernelStarts = nullptr;
    thread_local set<int64_t> *blocks = nullptr;

    bool blocksLabeled = false;
    vector<string> currentKernel;
    std::set<std::set<int64_t>> kernels;

    std::set<set<int64_t>> finalSets;
    mutex setMutex;

    void Setup()
    {
        uint64_t blockCount = 0;
        for (auto &mi : *bitcode)
        {
            for (auto fi = mi.begin(); fi != mi.end(); fi++)
            {
                blockCount++;
            }
        }

        if (blockCount == 0)
        {
            throw AtlasException("Found 0 blocks in bitcode");
        }

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
        for (const auto &kernel : kernels)
        {
            for (auto block : kernel)
            {
                kernelMap[block].insert(a);
            }
            a++;
        }
    }
    void Reset()
    {
        setMutex.lock();
        for (uint64_t i = 0; i < kernels.size(); i++)
        {
            finalSets.insert(finalBlocks[i]);
        }
        setMutex.unlock();
    }
    void Process(std::vector<std::string> &values)
    {
        string key = values[0];
        string value = values[1];
        if (key == "BBEnter")
        {
            int block = stoi(value, nullptr, 0);
            openCount[block]++; //mark this block as being entered
            openBlocks.insert(block);

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
        return finalSets;
    }
} // namespace TypeTwo