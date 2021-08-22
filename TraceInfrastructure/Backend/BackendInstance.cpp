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

class UniqueID
{
public:
    /// Unique idenfitfier
    uint64_t IID;
    UniqueID() = default;
    virtual ~UniqueID() = default;
    uint64_t getNextIID()
    {
        return nextIID++;
    }
protected:
    void setNextIID(uint64_t next)
    {
        if( next > nextIID )
        {
            nextIID = next + 1;
        }
    }
private:
    /// Counter for the next unique idenfitfier
    static uint64_t nextIID;
};

uint64_t UniqueID::nextIID = 0;

class CodeSection : public UniqueID
{
public:
    set<uint64_t> blocks;
    set<uint64_t> entrances;
    set<uint64_t> exits;
    virtual ~CodeSection();
};

CodeSection::~CodeSection() = default;

class Kernel : public CodeSection
{
public:
    string label;
    set<int> parents;
    set<int> children;
    vector<class KernelInstance *> instances;
    Kernel(){};
    Kernel(int id)
    {
        IID = (uint64_t)id;
        setNextIID(IID);
    }
};

class NonKernel : public CodeSection
{
public:
    vector<class NonKernelInstance*> instances;
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
        return lhs->IID < rhs->IID;
    }
    bool operator()(const Kernel *lhs, uint64_t rhs) const
    {
        return lhs->IID < rhs;
    }
    bool operator()(uint64_t lhs, const Kernel *rhs) const
    {
        return lhs < rhs->IID;
    }
};

class CodeInstance : public UniqueID
{
public:
    /// Position of this instance in the timeline of kernel instances
    uint32_t position;
    /// Counter for the number of times this kernel instance has occurred
    uint32_t iterations;
    virtual ~CodeInstance();
};

CodeInstance::~CodeInstance() = default;

/// Tracks the iteration of a kernel
class KernelInstance : public CodeInstance
{
public:
    /// Points to the kernel this iteration maps to
    const Kernel *k;
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

class NonKernelInstance : public CodeInstance
{
public:
    set<uint64_t> blocks;
    uint64_t firstBlock;
    NonKernel* nk;
    NonKernelInstance(uint64_t firstBlock)
    {
        IID = getNextIID();
        blocks.insert(firstBlock);
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

/// Holds all kernel instances we will be looking for in the profile
set<Kernel *, p_KCompare> kernels;
/// Holds all non-kernel instance we find in the profile
set<NonKernel*, p_UIDCompare> nonKernels;
/// Holds all kernels that are alive at a given moment
std::set<Kernel *, p_KCompare> liveKernels;
/// Holds the order of kernel instances measured while profiling (kernel IDs, instance index)
std::vector<pair<int, int>> TimeLine;
/// Current non kernel instance. If nullptr there is no current non-kernel instance
NonKernelInstance* currentNKI;
/// Remembers the block seen before the current so we can dynamically find kernel exits
uint64_t lastBlock;

/// On/off switch for the profiler
bool instanceActive = false;

void GenerateDot(const set<NonKernel *, p_UIDCompare> &nonKernels, const set<Kernel *, p_KCompare> &kernels)
{
    string dotString = "digraph{\n";
    // label kernels and nonkernels
    for (const auto &kernel : kernels)
    {
        dotString += "\t" + to_string(kernel->IID) + " [label=\"" + kernel->label + "\"]\n";
    }
    for (const auto &nk : nonKernels)
    {
        string nkLabel = "";
        auto blockIt = nk->blocks.begin();
        nkLabel += to_string(*blockIt);
        blockIt++;
        for( ; blockIt != nk->blocks.end(); blockIt++ )
        {
            nkLabel += "," + to_string(*blockIt);
        }
        dotString += "\t" + to_string(nk->IID) + " [label=\"" + nkLabel + "\"]\n";
    }
    // now build out the nodes in the graph
    // we only go to the second to last element because the last element has no outgoing edges
    for (unsigned int i = 0; i < TimeLine.size()-1; i++)
    {
        auto currentKernel = kernels.find(TimeLine[i].first);
        auto currentNonKernel = nonKernels.find(TimeLine[i].first);
        auto nextKernel = kernels.find(TimeLine[i+1].first);
        auto nextNonKernel = nonKernels.find(TimeLine[i+1].first);
        // curremtSection is a base pointer that describes the current node in the graph (the source node of the edge to be described)
        CodeSection* currentSection;
        if( (currentKernel != kernels.end()) && (currentNonKernel != nonKernels.end()) )
        {
            throw AtlasException("Overlap between kernel ID and nonKernel ID!");
        }
        else if( currentKernel != kernels.end() )
        {
            currentSection = *currentKernel;
            auto currentInstance = (*currentKernel)->instances[(unsigned int)TimeLine[i].second];
            // construct a map for all embedded kernels for this instance
            // don't use the timeline vector because it only allows for 1 child to each parent (the structures scale this to arbitrary number of children)
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
            for (auto UID = next(hierarchy.begin()); UID != hierarchy.end(); UID++)
            {
                dotString += "\t" + to_string((*UID)->IID) + " -> " + to_string( (*prev(UID))->IID ) + " [style=dashed] [label="+to_string((*UID)->iterations)+"];\n";
            }
        }
        else if( currentNonKernel != nonKernels.end() )
        {
            // nonkernel sections cannot have hierarchy
            currentSection = *currentNonKernel;
        }
        else
        {
            throw AtlasException("ID in the TimeLine mapped to neither a kernel nor a nonkernel!");
        }
        // nextSection is a base pointer describing the next node in the graph (the sink node of the edge to be described)
        CodeSection* nextSection;
        if( (nextKernel != kernels.end()) && (nextNonKernel != nonKernels.end()) )
        {
            throw AtlasException("Overlap between kernel ID and nonKernel ID!");
        }
        else if( nextKernel != kernels.end() )
        {
            nextSection = *nextKernel;
        }
        else if( nextNonKernel != nonKernels.end() )
        {
            nextSection = *nextNonKernel;
        }
        else
        {
            throw AtlasException("ID in the TimeLine mapped to neither a kernel nor a nonkernel!");
        }
        // TODO: add iteration count to the edge
        dotString += "\t" + to_string(currentSection->IID) + " -> " + to_string(nextSection->IID) + ";\n";//[label=" + to_string(currentSection->) + "];\n";

    }
    dotString += "}";

    // print file
    ofstream DAGStream("DAG.dot");
    DAGStream << dotString << "\n";
    DAGStream.close();
}

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
                newKernel->label = j["Kernels"][kid.key()]["Labels"].front();
                newKernel->blocks.insert(j["Kernels"][kid.key()]["Blocks"].begin(), j["Kernels"][kid.key()]["Blocks"].end());
                newKernel->parents.insert(j["Kernels"][kid.key()]["Parents"].begin(), j["Kernels"][kid.key()]["Parents"].end());
                newKernel->children.insert(j["Kernels"][kid.key()]["Children"].begin(), j["Kernels"][kid.key()]["Children"].end());
                kernels.insert(newKernel);
            }
        }
    }

    void InstanceDestroy()
    {
        // output data structure
        // first create an instance to kernel ID mapping
        // remember that this is hierarchical
        map<uint32_t, vector<UniqueID *>> TimeToInstances;
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
                vector<UniqueID *> hierarchy;
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
                TimeToInstances[i] = vector<UniqueID*>( (*currentNonKernel)->instances.begin(), (*currentNonKernel)->instances.end() );
            }
            else
            {
                throw AtlasException("ID in the TimeLine mapped to neither a kernel nor a nonkernel!");
            }
        }

        // output what happens at each time instance
        for (const auto &time : TimeToInstances)
        {
            instanceMap["Time"][to_string(time.first)] = vector<pair<int, int>>();
            for (auto UID : time.second)
            {
                if( auto instance = dynamic_cast<KernelInstance*>(UID) )
                {
                    instanceMap["Time"][to_string(time.first)].push_back(pair<int, int>(instance->k->IID, instance->iterations));
                }
                else if( auto nkinstance = dynamic_cast<NonKernelInstance*>(UID) )
                {
                    instanceMap["Time"][to_string(time.first)].push_back(pair<int, int>(nkinstance->nk->IID, nkinstance->iterations));
                }
            }
        }

        // output kernel instances for completeness
        for( const auto& k : kernels )
        {
            instanceMap["Kernels"][to_string(k->IID)]["Blocks"] = vector<uint64_t>(k->blocks.begin(), k->blocks.end());
            instanceMap["Kernels"][to_string(k->IID)]["Entrances"] = vector<uint64_t>(k->entrances.begin(), k->entrances.end());
            instanceMap["Kernels"][to_string(k->IID)]["Exits"] = vector<uint64_t>(k->exits.begin(), k->exits.end());
        }
        // output the nonkernel instances we found
        for( const auto& nk : nonKernels )
        {
            instanceMap["NonKernels"][to_string(nk->IID)]["Blocks"] = vector<uint64_t>(nk->blocks.begin(), nk->blocks.end());
            instanceMap["NonKernels"][to_string(nk->IID)]["Entrances"] = vector<uint64_t>(nk->entrances.begin(), nk->entrances.end());
            instanceMap["NonKernels"][to_string(nk->IID)]["Exits"] = vector<uint64_t>(nk->exits.begin(), nk->exits.end());
        }

        // print the file
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

        // generate dot file for the DAG
        GenerateDot(nonKernels, kernels);

        // free our stuff
        for (auto entry : kernels)
        {
            for (auto instance : entry->instances)
            {
                delete instance;
            }
            delete entry;
        }
        for( auto entry : nonKernels )
        {
            for( auto instance : entry->instances )
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
                    if( currentNKI )
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
                            nonKernels.insert(match);
                        }
                        TimeLine.push_back( pair<int, int>(match->IID, match->instances.size() ) );
                        // look to see if we have an instance exactly like this one already in the nonkernel
                        for( const auto& instance : match->instances )
                        {
                            if( instance->firstBlock == currentNKI->firstBlock )
                            {
                                // we already have this instance
                                delete currentNKI;
                                instance->iterations++;
                                currentNKI = nullptr;
                            }
                        }
                        if( currentNKI != nullptr )
                        {
                            currentNKI->nk = match;
                            // push the new instance
                            match->instances.push_back(currentNKI);
                            match->blocks.insert(currentNKI->blocks.begin(), currentNKI->blocks.end());
                            match->entrances.insert(currentNKI->firstBlock);
                            match->exits.insert(lastBlock);
                        }
                        currentNKI = nullptr;
                    }
                    // if this kernel is the top level kernel insert it into the timeline
                    if (kern->parents.empty())
                    {
                        TimeLine.push_back(pair<int, int>(kern->IID, kern->instances.size()));
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
                currentNKI->firstBlock = a;
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