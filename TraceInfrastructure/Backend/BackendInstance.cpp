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

struct UniqueID
{
    /// Unique idenfitfier
    uint64_t IID;
    /// Counter for the next unique idenfitfier
    static uint64_t nextIID;
    uint64_t getNextIID()
    {
        return nextIID++;
    }
};

uint64_t UniqueID::nextIID = 0;

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

struct NonKernel : UniqueID
{
    set<uint64_t> blocks;
    vector<struct NonKernelInstance*> instances;
    NonKernel()
    {
        IID = getNextIID();
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
struct KernelInstance : UniqueID
{
    /// Points to the kernel this iteration maps to
    const Kernel *k;
    /// Position of this instance in the timeline of kernel instances
    uint32_t position;
    /// Counter for the number of times this kernel instance has occurred
    uint32_t iterations;
    /// Array of child kernels that are called by this kernel.
    /// These children are known to be called at runtime by this kernel in this order
    /// Noting that this structure is the parent structure, the children array can encode arbitrary hierarchical depths of child kernels (ie this array can contain arrays of children)
    set<KernelInstance *> children;
    KernelInstance(Kernel *kern)
    {
        IID = getNextIID();
        k = kern;
        iterations = 0;
        position = (uint32_t)kern->instances.size();
        kern->instances.push_back(this);
        children = set<KernelInstance *>();
    }
};

struct p_UIDCompare
{
    using is_transparent = void;
    bool operator()(const UniqueID *lhs, const UniqueID *rhs) const
    {
        return lhs->IID < rhs->IID;
    }
    bool operator()(const UniqueID *lhs, uint64_t rhs) const
    {
        return lhs->IID < rhs;
    }
    bool operator()(uint64_t lhs, const UniqueID *rhs) const
    {
        return lhs < rhs->IID;
    }
};

struct NonKernelInstance : UniqueID
{
    set<uint64_t> blocks;
    NonKernel* nk;
    NonKernelInstance(uint64_t firstBlock)
    {
        IID = getNextIID();
        blocks.insert(firstBlock);
    }
};

/// Holds all kernel instances we will be looking for in the profile
set<Kernel *, p_KCompare> kernels;
/// Holds all non-kernel instance we find in the profile
set<NonKernel*, p_UIDCompare> nonKernels;
/// Holds all kernels that are alive at a given moment
std::set<Kernel *, p_KCompare> liveKernels;
/// Holds the order of kernel instancesmeasured while profiling (kernel IDs, instance index)
std::vector<pair<int, int>> TimeLine;
/// Current non kernel instance. If nullptr there is no current non-kernel instance
NonKernelInstance* currentNKI;
/// Remembers the block seen before the current so we can dynamically find kernel exits
uint64_t lastBlock;

/// On/off switch for the profiler
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
                newKernel->blocks.insert(j["Kernels"][kid.key()]["Blocks"].begin(), j["Kernels"][kid.key()]["Blocks"].end());
                newKernel->parents.insert(j["Kernels"][kid.key()]["Parents"].begin(), j["Kernels"][kid.key()]["Parents"].end());
                newKernel->children.insert(j["Kernels"][kid.key()]["Children"].begin(), j["Kernels"][kid.key()]["Children"].end());
                kernels.insert(newKernel);
                if( (uint64_t)newKernel->ID > UniqueID::nextIID )
                {
                    UniqueID::nextIID = (uint64_t)newKernel->ID + 1;
                }
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
            auto currentKernel = kernels.find(TimeLine[i].first);
            auto currentNonKernel = nonKernels.find(TimeLine[i].first);
            if( (currentKernel != kernels.end()) && (currentNonKernel != nonKernels.end()) )
            {
                throw AtlasException("Overlap between kernel ID and nonKernel ID!");
            }
            else if( currentKernel != kernels.end() )
            {
                // construct a map for all embedded kernels for this instance
                auto currentInstance = (*currentKernel)->instances[(unsigned int)TimeLine[i].second];
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
            else if( currentNonKernel != nonKernels.end() )
            {
                // stuff
            }
            else
            {
                throw AtlasException("Could not map the ID in the TimeLine to neither a kernel nor a nonkernel!");
            }
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
                    if( enteredKernel != nullptr )
                    {
                        throw AtlasException("We have multiple kernel entrances that map to this block!");
                    }
                    enteredKernel = kern;
                    liveKernels.insert(kern);
                    kern->entrances.insert(a);
                    // take care of any serial code that occurred before our kernel
                    if( !currentNKI )
                    {
                        // first we have to find out whether or not this nonKernel has been seen before
                        // the only way to do this is to go through all non-kernel instances and find one whose block set matches this one
                        NonKernel* match = nullptr;
                        for( const auto& nk : nonKernels )
                        {
                            if( nk->blocks == currentNKI->blocks )
                            {
                                match = nk;
                                break;
                            }
                        }
                        if( !match )
                        {
                            match = new NonKernel();
                        }
                        match->instances.push_back(currentNKI);
                        currentNKI->nk = match;
                        currentNKI = nullptr;
                        TimeLine.push_back( pair<int, int>(match->IID, match->instances.size() ) );
                    }
                    // if this kernel is the top level kernel insert it into the timeline
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
                new KernelInstance(enteredKernel);
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
                    auto newInstance = new KernelInstance(enteredKernel);
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
            if( currentNKI )
            {
                currentNKI->blocks.insert(a);
            }
            else
            {
                currentNKI = new NonKernelInstance(a);
            }
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