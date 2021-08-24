#include "AtlasUtil/Exceptions.h"
#include "Backend/Kernel.h"
#include "Backend/KernelInstance.h"
#include "Backend/NonKernel.h"
#include "Backend/NonKernelInstance.h"
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

/// Holds all CodeSection instances (each member is polymorphic to either kernels or nonkernels)
set<CodeSection *, p_UIDCompare> codeSections;
/// Reverse mapping from block to CodeSection
map<uint64_t, set<CodeSection *, p_UIDCompare>> blockToSection;
/// Holds all kernel instances
set<Kernel *, p_UIDCompare> kernels;
// Holds all nonkernel instances
set<NonKernel *, p_UIDCompare> nonKernels;
/// Holds all kernels that are currently alive
std::set<Kernel *, p_UIDCompare> liveKernels;
/// Holds the order of kernel instances measured while profiling (kernel IDs, instance index)
std::vector<pair<int, int>> TimeLine;
/// Current non kernel instance. If nullptr there is no current non-kernel instance
NonKernelInstance *currentNKI;
/// Remembers the block seen before the current so we can dynamically find kernel exits
uint64_t lastBlock;

/// On/off switch for the profiler
bool instanceActive = false;

void GenerateDot(const set<CodeSection *, p_UIDCompare> &codeSections)
{
    string dotString = "digraph{\n";
    // label kernels and nonkernels
    for (const auto &cs : codeSections)
    {
        if (auto kernel = dynamic_cast<Kernel *>(cs))
        {
            for (const auto &instance : kernel->getInstances())
            {
                dotString += "\t" + to_string(instance->IID) + " [label=\"" + kernel->label + "\"]\n";
            }
        }
        else if (auto nk = dynamic_cast<NonKernel *>(cs))
        {
            string nkLabel = "";
            auto blockIt = nk->blocks.begin();
            nkLabel += to_string(*blockIt);
            blockIt++;
            for (; blockIt != nk->blocks.end(); blockIt++)
            {
                nkLabel += "," + to_string(*blockIt);
            }
            for (const auto &instance : nk->getInstances())
            {
                dotString += "\t" + to_string(instance->IID) + " [label=\"" + nkLabel + "\"]\n";
            }
        }
        else
        {
            throw AtlasException("CodeSection mapped to neither a Kernel nor a Nonkernel!");
        }
    }

    // now build out the nodes in the graph
    // we only go to the second to last element because the last element has no outgoing edges
    for (unsigned int i = 0; i < TimeLine.size() - 1; i++)
    {
        auto currentSectionIt = codeSections.find(TimeLine[i].first);
        if (currentSectionIt == codeSections.end())
        {
            throw AtlasException("currentSection in the timeline does not map to an exsting code section!");
        }
        auto currentSection = *currentSectionIt;
        auto nextSectionIt = codeSections.find(TimeLine[i + 1].first);
        if (nextSectionIt == codeSections.end())
        {
            throw AtlasException("currentSection in the timeline does not map to an exsting code section!");
        }
        auto nextSection = *nextSectionIt;
        // two steps here
        // first, define the hierarchy for the current node if this is a kernel
        // second, define the currentInstance
        CodeInstance *currentInstance;
        if (auto currentKernel = dynamic_cast<Kernel *>(currentSection))
        {
            // step 1
            // construct a map for all embedded kernels for this instance
            // don't use the timeline vector because it only allows for 1 child to each parent (the structures scale this to arbitrary number of children)
            std::deque<KernelInstance *> Q;
            vector<KernelInstance *> hierarchy;
            Q.push_front(currentKernel->getInstance((unsigned int)TimeLine[i].second));
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
                dotString += "\t" + to_string((*UID)->IID) + " -> " + to_string((*prev(UID))->IID) + " [style=dashed] [label=" + to_string((*UID)->iterations) + "];\n";
            }
            // step 2
            currentInstance = currentKernel->getInstance(TimeLine[i].second);
        }
        else if (auto currentNonKernel = dynamic_cast<NonKernel *>(currentSection))
        {
            currentInstance = currentNonKernel->getInstance(TimeLine[i].second);
        }
        else
        {
            throw AtlasException("currentSection casts to neither a kernel nor a non-kernel!");
        }
        // now find the correct kernel instances from the timeline and encode the edge connecting them
        CodeInstance *nextInstance;
        if (auto nextKernel = dynamic_cast<Kernel *>(nextSection))
        {
            nextInstance = nextKernel->getInstance(TimeLine[i].second);
        }
        else if (auto nextNonKernel = dynamic_cast<NonKernel *>(nextSection))
        {
            nextInstance = nextNonKernel->getInstance(TimeLine[i].second);
        }
        else
        {
            throw AtlasException("nextSection maps to neither a kernel nor a non-kernel!");
        }
        // TODO: add iteration count to the edge
        dotString += "\t" + to_string(currentInstance->IID) + " -> " + to_string(nextInstance->IID) + ";\n"; //[label=" + to_string(currentSection->) + "];\n";
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
                codeSections.insert(newKernel);
                kernels.insert(newKernel);
                for (const auto &b : newKernel->blocks)
                {
                    blockToSection[b].insert(newKernel);
                }
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
            auto currentSegment = codeSections.find(TimeLine[i].first);
            if (currentSegment == codeSections.end())
            {
                throw AtlasException("The current timeline entry does not map to an existing code segment!");
            }
            if (auto currentKernel = dynamic_cast<Kernel *>(*currentSegment))
            {
                // construct a map for all embedded kernels for this instance
                std::deque<KernelInstance *> Q;
                vector<UniqueID *> hierarchy;
                Q.push_front(currentKernel->getInstance((unsigned int)TimeLine[i].second));
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
            else if (auto currentNonKernel = dynamic_cast<NonKernel *>(*currentSegment))
            {
                TimeToInstances[i] = vector<UniqueID *>();
                for (const auto &j : currentNonKernel->getInstances())
                {
                    TimeToInstances[i].push_back(j);
                }
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
                if (auto instance = dynamic_cast<KernelInstance *>(UID))
                {
                    instanceMap["Time"][to_string(time.first)].push_back(pair<int, int>(instance->k->IID, instance->iterations));
                }
                else if (auto nkinstance = dynamic_cast<NonKernelInstance *>(UID))
                {
                    instanceMap["Time"][to_string(time.first)].push_back(pair<int, int>(nkinstance->nk->IID, nkinstance->iterations));
                }
            }
        }

        // output kernel instances for completeness and non-kernel instances for reference
        for (const auto &cs : codeSections)
        {
            if (auto k = dynamic_cast<Kernel *>(cs))
            {
                instanceMap["Kernels"][to_string(k->IID)]["Blocks"] = vector<uint64_t>(k->blocks.begin(), k->blocks.end());
                instanceMap["Kernels"][to_string(k->IID)]["Entrances"] = vector<uint64_t>(k->entrances.begin(), k->entrances.end());
                instanceMap["Kernels"][to_string(k->IID)]["Exits"] = vector<uint64_t>(k->exits.begin(), k->exits.end());
            }
            else if (auto nk = dynamic_cast<NonKernel *>(cs))
            {
                instanceMap["NonKernels"][to_string(nk->IID)]["Blocks"] = vector<uint64_t>(nk->blocks.begin(), nk->blocks.end());
                instanceMap["NonKernels"][to_string(nk->IID)]["Entrances"] = vector<uint64_t>(nk->entrances.begin(), nk->entrances.end());
                instanceMap["NonKernels"][to_string(nk->IID)]["Exits"] = vector<uint64_t>(nk->exits.begin(), nk->exits.end());
            }
            else
            {
                throw AtlasException("CodeSegment pointer is neither a kernel nor a nonkernel!");
            }
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
        GenerateDot(codeSections);

        // free our stuff
        for (auto entry : codeSections)
        {
            for (auto instance : entry->getInstances())
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
        for (auto &cs : blockToSection[a])
        {
            if (auto kern = dynamic_cast<Kernel *>(cs))
            {
                if (liveKernels.find(kern) == liveKernels.end())
                {
                    if (kern->blocks.find((uint32_t)a) != kern->blocks.end())
                    {
                        if (enteredKernel != nullptr)
                        {
                            throw AtlasException("We have multiple kernel entrances that map to this block!");
                        }
                        enteredKernel = kern;
                        liveKernels.insert(kern);
                        kern->entrances.insert(a);
                        // take care of any serial code that occurred before our kernel
                        if (currentNKI)
                        {
                            // first we have to find out whether or not this nonKernel has been seen before
                            // the only way to do this is to go through all non-kernel instances and find one whose block set matches this one
                            NonKernel *match = nullptr;
                            for (const auto &nk : nonKernels)
                            {
                                if (nk->blocks == currentNKI->blocks)
                                {
                                    match = nk;
                                    break;
                                }
                            }
                            if (!match)
                            {
                                match = new NonKernel();
                                codeSections.insert(match);
                                nonKernels.insert(match);
                                for (const auto &b : match->blocks)
                                {
                                    blockToSection[b].insert(match);
                                }
                            }
                            TimeLine.push_back(pair<int, int>(match->IID, match->getInstances().size()));
                            // look to see if we have an instance exactly like this one already in the nonkernel
                            for (const auto &instance : match->getInstances())
                            {
                                if (instance->firstBlock == currentNKI->firstBlock)
                                {
                                    // we already have this instance
                                    delete currentNKI;
                                    instance->iterations++;
                                    currentNKI = nullptr;
                                }
                            }
                            if (currentNKI != nullptr)
                            {
                                currentNKI->nk = match;
                                // push the new instance
                                match->addInstance(currentNKI);
                                match->blocks.insert(currentNKI->blocks.begin(), currentNKI->blocks.end());
                                match->entrances.insert(currentNKI->firstBlock);
                                match->exits.insert(lastBlock);
                            }
                            currentNKI = nullptr;
                        }
                        // if this kernel is the top level kernel insert it into the timeline
                        if (kern->parents.empty())
                        {
                            TimeLine.push_back(pair<int, int>(kern->IID, kern->getInstances().size()));
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
                        kern->getCurrentInstance()->iterations++;
                    }
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
                auto parent = dynamic_cast<Kernel *>(*codeSections.find(*(enteredKernel->parents.begin())));
                auto parentInstance = parent->getCurrentInstance();
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
            if (currentNKI)
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