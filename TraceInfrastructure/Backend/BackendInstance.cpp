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
// 1. runtime encoding of the counts of these instances (Done, 8/17/21)
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
    vector<struct KernelInstance *> instances;
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
struct KernelInstance
{
    /// Unique idenfitfier
    uint64_t IID;
    /// Points to the kernel this iteration maps to
    const Kernel *k;
    /// Position of this instance in the timeline of kernel instances
    uint32_t position;
    /// Counter for the number of times this kernel instance has occurred
    uint32_t iterations;
    /// Size of the children array
    uint8_t childCount;
    /// Array of child kernels that are called by this kernel.
    /// These children are known to be called at runtime by this kernel in this order
    /// Noting that this structure is the parent structure, the children array can encode arbitrary hierarchical depths of child kernels (ie this array can contain arrays of children)
    set<KernelInstance *> children;
    KernelInstance(Kernel *kern, std::set<Kernel *, p_KCompare> kernels)
    {
        IID = nextIID++;
        k = kern;
        iterations = 0;
        position = (uint32_t)kern->instances.size();
        kern->instances.push_back(this);
        childCount = 0;
        // during initialization we encode how many children have been initialized through childCount
        children = set<KernelInstance *>();
    }
    static uint64_t nextIID;
};

uint64_t KernelInstance::nextIID = 0;

struct p_KICompare
{
    using is_transparent = void;
    bool operator()(const KernelInstance *lhs, const KernelInstance *rhs) const
    {
        return lhs->k->ID < rhs->k->ID;
    }
    bool operator()(const KernelInstance *lhs, uint64_t rhs) const
    {
        return lhs->IID < rhs;
    }
    bool operator()(uint64_t lhs, const KernelInstance *rhs) const
    {
        return lhs < rhs->IID;
    }
};

/// Holds all kernel instances we will be looking for in the profile
set<Kernel *, p_KCompare> kernels;
/// Holds all kernels that are alive at a given moment
std::set<Kernel *, p_KCompare> liveKernels;
/// Holds the order of kernel instancesmeasured while profiling (kernel IDs, instance index)
std::vector<pair<int, int>> TimeLine;
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
                newKernel->parents.insert(j["Kernels"][kid.key()]["Parents"].begin(), j["Kernels"][kid.key()]["Parents"].end());
                newKernel->children.insert(j["Kernels"][kid.key()]["Children"].begin(), j["Kernels"][kid.key()]["Children"].end());
                kernels.insert(newKernel);
            }
        }
    }

    void InstanceDestroy()
    {
        // output data structure here
        // first create an instance to kernel ID mapping
        // remember that this is hierarchical
        map<uint32_t, vector<KernelInstance *>> TimeToInstances;
        json instanceMap;
        for (uint32_t i = 0; i < TimeLine.size(); i++)
        {
            auto currentKernel = *kernels.find(TimeLine[i].first);
            auto currentInstance = currentKernel->instances[TimeLine[i].second];
            // construct a map for all embedded kernels for this instance
            std::deque<KernelInstance *> Q;
            vector<KernelInstance *> hierarchy;
            Q.push_front(currentInstance);
            while (!Q.empty())
            {
                for (auto child = Q.front()->children.begin(); child != Q.front()->children.end(); child++)
                {
                    Q.push_back(*child);
                }
                hierarchy.push_back(Q.front());
                Q.pop_front();
            }
            TimeToInstances[i] = hierarchy;
        }

        for (const auto &time : TimeToInstances)
        {
            instanceMap[to_string(time.first)] = vector<pair<int, int>>();
            for (const auto &instance : time.second)
            {
                instanceMap[to_string(time.first)].push_back(pair<int, int>(instance->k->ID, instance->iterations));
            }
        }
        ofstream file;
        char *instanceFileName = getenv("INSTANCE_FILE");
        if (instanceFileName == nullptr)
        {
            file.open("Instance.json");
        }
        else
        {
            file.open(instanceFileName);
        }
        file << setw(4) << instanceMap;

        // free our stuff
        for (auto entry : kernels)
        {
            for (auto instance : entry->instances)
            {
                delete instance;
            }
            delete entry;
        }
        instanceActive = false;
    }

    void InstanceIncrement(uint64_t a)
    {
        if (!instanceActive)
        {
            return;
        }
        Kernel *enteredKernel = nullptr;
        // first step, acquire all kernels who have this block in them
        // we must process the parent kernels first
        for (auto &kern : kernels)
        {
            if (liveKernels.find(kern) == liveKernels.end())
            {
                if (kern->blocks.find((uint32_t)a) != kern->blocks.end())
                {
                    enteredKernel = kern;
                    liveKernels.insert(kern);
                    kern->entrances.insert(a);
                    if (kern->parents.empty())
                    {
                        TimeLine.push_back(pair<int, int>(kern->ID, kern->instances.size()));
                    }
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
                else if (kern->entrances.find(a) != kern->entrances.end())
                {
                    // we have made a revolution within this kernel, so update its iteration count
                    kern->instances.back()->iterations++;
                }
            }
        }
        // now make a new kernel instance if necessary
        if (enteredKernel)
        {
            // instances are in the eye of the parent
            if (enteredKernel->parents.empty())
            {
                auto newInstance = new KernelInstance(enteredKernel, kernels);
            }
            else if (enteredKernel->parents.size() == 1)
            {
                // if we already have an instance for this child in the parent we don't make a new one
                auto parent = *kernels.find(*(enteredKernel->parents.begin()));
                auto parentInstance = parent->instances.back();
                bool childFound = false;
                for (auto child : parentInstance->children)
                {
                    if (child->k == enteredKernel)
                    {
                        childFound = true;
                    }
                }
                if (!childFound)
                {
                    // we don't have an instance for this child yet, create one
                    auto newInstance = new KernelInstance(enteredKernel, kernels);
                    parentInstance->children.insert(newInstance);
                }
            }
            else
            {
                throw AtlasException("Don't know what to do about finding the current kernel instance when there is more than one parent!");
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
        instanceActive = true;
        InstanceIncrement(a);
    }
}