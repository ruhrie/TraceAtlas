#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// TODO
// 1. runtime encoding of the counts of these instances
// 2. non-kernel code sections

#define STACK_SIZE 1000

using namespace std;
using json = nlohmann::json;

struct KernelInstance
{
    int ID;
    set<uint32_t> blocks;
    set<uint32_t> entrances;
    set<uint32_t> exits;
    set<int> parents;
    set<int> children;
    KernelInstance(){};
    KernelInstance(int id)
    {
        ID = id;
    }
};

struct KICompare
{
    using is_transparent = void;
    bool operator()(const KernelInstance &lhs, const KernelInstance &rhs) const
    {
        return lhs.ID < rhs.ID;
    }
    bool operator()(const KernelInstance &lhs, int rhs) const
    {
        return lhs.ID < rhs;
    }
    bool operator()(int lhs, const KernelInstance &rhs) const
    {
        return lhs < rhs.ID;
    }
};

/// Holds all kernel instances we will be looking for in the profile
set<KernelInstance, KICompare> kernels;
/// LIFO that holds IDs of kernels that are currently active
/// Empty means no kernel is currently active
/// The index always points to the slot of the next entry
uint32_t stackIndex = 0;
uint64_t seenKernels[STACK_SIZE];

void pushStack(int KID)
{
    if (stackIndex < STACK_SIZE)
    {
        seenKernels[stackIndex] = KID;
        stackIndex++;
    }
    else
    {
        cout << "Critical: Kernel instance stack has overflowed!" << endl;
        exit(EXIT_FAILURE);
    }
}

int popStack()
{
    if (stackIndex > 0)
    {
        stackIndex--;
        return seenKernels[stackIndex + 1];
    }
    return seenKernels[0];
}

/// Flag initiating the profiler
bool instanceActive = false;

extern "C"
{
    void ReadKernelFile()
    {
        const char *kfName = getenv("KERNEL_FILE");
        if (!kfName)
        {
            kfName = &"kernel.json"[0];
        }
        std::ifstream inputJson;
        json j;
        try
        {
            inputJson.open(kfName);
            inputJson >> j;
            inputJson.close();
        }
        catch (std::exception &e)
        {
            cout << "Critical: Couldn't open kernel file: " + string(kfName) << endl;
            cout << e.what() << endl;
            exit(EXIT_FAILURE);
        }
        for (const auto &bbid : j.items())
        {
            if (j[bbid.key()].find("Kernels") != j[bbid.key()].end())
            {
                for (const auto &kid : j[bbid.key()]["Kernels"].items())
                {
                    auto newKernel = KernelInstance(stoi(kid.key()));
                    // need blocks, entrances, exits, children and parents
                    newKernel.blocks.insert(j[bbid.key()]["Kernels"][kid.key()]["Blocks"].begin(), j[bbid.key()]["Kernels"][kid.key()]["Blocks"].end());
                    kernels.insert(newKernel);
                }
            }
        }
    }

    void InstanceDestroy()
    {
        // do nothing for now
    }

    void InstanceIncrement(uint64_t a)
    {
        if (!instanceActive)
        {
            return;
        }
        // first step, acquire all kernels who have this block in them
        // if we don't find any live kernels it means we are in non-kernel code
        std::set<KernelInstance, KICompare> liveKernels;
        for (const auto &kern : kernels)
        {
            if (kern.blocks.find((uint32_t)a) != kern.blocks.end())
            {
                liveKernels.insert(kern);
            }
        }
        if (liveKernels.empty())
        {
            // we are in serial code. do nothing for now
        }
        // second step, pick out the kernels that are parents of the current live kernel
        KernelInstance activeKernel;
        for (const auto &kern : liveKernels)
        {
            if (!kern.children.empty())
            {
                bool foundChild = false;
                for (const auto &child : kern.children)
                {
                    if (liveKernels.find(child) != liveKernels.end())
                    {
                        // this is a parent kernel, move on
                        foundChild = true;
                        break;
                    }
                }
                if (foundChild)
                {
                    continue;
                }
                else
                {
                    // this kernel has no children that are live, it must be the live kernel
                    activeKernel = kern;
                    break;
                }
            }
            // else we have found the one
            else
            {
                activeKernel = kern;
                break;
            }
        }
        // third, identify if we just entered or exited the kernel and modify the stack and RLE accordingly
        auto foundBlock = false;
        for (const auto &block : activeKernel.entrances)
        {
            if (block == (uint32_t)a)
            {
                pushStack(activeKernel.ID);
                foundBlock = true;
                break;
            }
        }
        if (foundBlock)
        {
            return;
        }
        for (const auto &block : activeKernel.exits)
        {
            if (block == (uint32_t)a)
            {
                popStack();
            }
        }
    }

    void InstanceInit(uint64_t a)
    {
        ReadKernelFile();
        instanceActive = true;
        InstanceIncrement(a);
    }
}