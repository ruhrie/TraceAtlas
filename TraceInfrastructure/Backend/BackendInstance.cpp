#include "AtlasUtil/Exceptions.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <queue>
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

struct Kernel
{
    int ID;
    set<uint32_t> blocks;
    set<uint64_t> entrances;
    set<uint64_t> exits;
    set<int> parents;
    set<int> children;
    Kernel(){};
    Kernel(int id)
    {
        ID = id;
    }
};

struct p_KCompare
{
    using is_transparent = void;
    bool operator()(const Kernel *lhs, const Kernel *rhs) const
    {
        return lhs->ID < rhs->ID;
    }
    bool operator()(const Kernel *lhs, int rhs) const
    {
        return lhs->ID < rhs;
    }
    bool operator()(int lhs, const Kernel *rhs) const
    {
        return lhs < rhs->ID;
    }
};

/// Tracks the iteration of a kernel
struct KernelIteration
{
    /// Points to the kernel this iteration maps to
    const Kernel *k;
    /// Counter for the number of times this kernel has occurred
    uint32_t iteration;
    /// Size of the children array
    uint8_t childCount;
    /// Array of child kernels that are called by this kernel.
    /// These children are known to be called at runtime by this kernel in this order
    /// Noting that this structure is the parent structure, the children array can encode arbitrary hierarchical depths of child kernels (ie this array can contain arrays of children)
    struct KernelIteration *children;
    KernelIteration(const Kernel *kern, std::set<Kernel *, p_KCompare> kernels)
    {
        k = kern;
        iteration = 0;
        childCount = 0;
        // during initialization we encode how many children have been initialized through childCount
        children = (KernelIteration *)calloc(kern->children.size(), sizeof(KernelIteration));
        std::deque<KernelIteration *> Q;
        Q.push_back(this);
        while (!Q.empty())
        {
            for (const auto &childID : Q.front()->k->children)
            {
                auto child = *kernels.find(childID);
                Q.front()->children[Q.front()->childCount].k = child;
                if (!child->children.empty())
                {
                    Q.front()->children[Q.front()->childCount].children = (KernelIteration *)calloc(child->children.size(), sizeof(KernelIteration));
                    Q.push_back(&Q.front()->children[Q.front()->childCount]);
                }
                Q.front()->childCount++;
            }
            Q.pop_front();
        }
    }
};

struct p_KICompare
{
    using is_transparent = void;
    bool operator()(const KernelIteration *lhs, const KernelIteration *rhs) const
    {
        return lhs->k->ID < rhs->k->ID;
    }
    bool operator()(const KernelIteration *lhs, int rhs) const
    {
        return lhs->k->ID < rhs;
    }
    bool operator()(int lhs, const KernelIteration *rhs) const
    {
        return lhs < rhs->k->ID;
    }
};

/// Holds all kernel instances we will be looking for in the profile
set<Kernel *, p_KCompare> kernels;
/// Holds all kernels that are alive at a given moment
std::set<Kernel *, p_KCompare> liveKernels;
/// Holds an entry for all kernel objects
std::set<KernelIteration *, p_KICompare> iterations;
/// Holds the order of kernels measured while profiling (kernel IDs)
std::vector<int> TimeLine;
/// Remembers the block seen before the current so we can dynamically find kernel exits
uint64_t lastBlock;

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
        if (j.find("Kernels") != j.end())
        {
            for (const auto &kid : j["Kernels"].items())
            {
                auto newKernel = new Kernel(stoi(kid.key()));
                // need blocks, entrances, exits, children and parents
                newKernel->blocks.insert(j["Kernels"][kid.key()]["Blocks"].begin(), j["Kernels"][kid.key()]["Blocks"].end());
                //newKernel->entrances.insert(j["Kernels"][kid.key()]["Entrances"].begin(), j["Kernels"][kid.key()]["Entrances"].end());
                //newKernel->exits.insert(j["Kernels"][kid.key()]["Exits"].begin(), j["Kernels"][kid.key()]["Exits"].end());
                newKernel->parents.insert(j["Kernels"][kid.key()]["Parents"].begin(), j["Kernels"][kid.key()]["Parents"].end());
                newKernel->children.insert(j["Kernels"][kid.key()]["Children"].begin(), j["Kernels"][kid.key()]["Children"].end());
                kernels.insert(newKernel);
            }
        }
    }

    void IncrementCounts()
    {
        // update the kernel instance structures
        // all active kernels should have a hierarchical relationship
        // in the KernelInstance structure, this hierarchy starts with the parent
        // Thus we need to find the parent-most kernel, then recurse down till we find the active child in the hierarchy
        // First, find the parent-most kernel
        Kernel *parentMost = nullptr;
        for (auto live : liveKernels)
        {
            if (live->parents.empty())
            {
                parentMost = live;
            }
        }
        if (parentMost == nullptr)
        {
            throw AtlasException("Could not find parent kernel!");
        }
        // Second, recurse until we find a child kernel who has no active children (or no children at all) and update its iteration count
        auto parentInstance = *iterations.find(parentMost->ID);
        auto currentParent = parentInstance;
        while (true)
        {
            if (currentParent->childCount)
            {
                bool foundChild = false;
                for (unsigned int i = 0; i < currentParent->childCount; i++)
                {
                    if (liveKernels.find(currentParent->children[i].k->ID) != liveKernels.end())
                    {
                        currentParent = &currentParent->children[i];
                        foundChild = true;
                        break;
                    }
                }
                if (!foundChild)
                {
                    // we must be the lowest in the tree but not a leaf. Update our structure
                    currentParent->iteration++;
                    break;
                }
            }
            else
            {
                // we have found the kerneliteration to update
                currentParent->iteration++;
                break;
            }
        }
    }

    void InstanceDestroy()
    {
        // output data structure here
        // construct BlockInfo json output
        json labelMap;
        /*
        for (uint32_t i = 0; i < callerHashTable->getFullSize(callerHashTable); i++)
        {
            for (uint32_t j = 0; j < callerHashTable->array[i].popCount; j++)
            {
                auto entry = callerHashTable->array[i].tuple[j];
                char label[100];
                sprintf(label, "%d", entry.label.blocks[0]);
                labelMap[string(label)]["BlockCallers"].push_back(entry.callee.blocks[1]);
            }
        }
        */
        ofstream file;
        char *labelFileName = getenv("BLOCK_FILE");
        if (labelFileName == nullptr)
        {
            file.open("BlockInfo.json");
        }
        else
        {
            file.open(labelFileName);
        }
        file << setw(4) << labelMap;

        // free our stuff
        for (auto entry : kernels)
        {
            delete entry;
        }
        for (auto entry : iterations)
        {
            // need to recursively free all children then the entry
        }
        instanceActive = false;
    }

    void InstanceIncrement(uint64_t a)
    {
        if (!instanceActive)
        {
            return;
        }
        // first step, acquire all kernels who have this block in them
        // we must process the parent kernels first
        for (auto &kern : kernels)
        {
            if (liveKernels.find(kern) == liveKernels.end())
            {
                if (kern->blocks.find((uint32_t)a) != kern->blocks.end())
                {
                    liveKernels.insert(kern);
                    kern->entrances.insert(a);
                    if (kern->parents.empty())
                    {
                        TimeLine.push_back(kern->ID);
                    }
                    else
                    {
                        std::deque<Kernel *> Q;
                        Q.push_front(kern);
                        while (!Q.empty())
                        {
                            for (const auto &p : Q.front()->parents)
                            {
                                auto parent = *kernels.find(p);
                                liveKernels.insert(parent);
                                if (!parent->parents.empty())
                                {
                                    Q.push_back(parent);
                                }
                            }
                            Q.pop_front();
                        }
                    }
                    IncrementCounts();
                }
            }
            else
            {
                if (kern->blocks.find((uint32_t)a) == kern->blocks.end())
                {
                    // this is an exit for this kernel
                    kern->exits.insert(lastBlock);
                    liveKernels.erase(kern);
                }
            }
        }
        // if we don't find any live kernels it means we are in non-kernel code
        if (liveKernels.empty())
        {
            // we are in serial code. do nothing for now
        }
        lastBlock = a;
    }

    void InstanceInit(uint64_t a)
    {
        ReadKernelFile();
        for (const auto &kern : kernels)
        {
            iterations.insert(new KernelIteration(kern, kernels));
        }
        instanceActive = true;
        InstanceIncrement(a);
    }
}