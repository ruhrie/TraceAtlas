#include "Backend/DashHashTable.h"
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

// TODO: runtime encoding of the counts of these instances

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
    KernelInstance(int id)
    {
        ID = id;
    }
}

/// Holds all kernel instances we will be looking for in the profile
set<struct KernelInstance> kernels;
/// LIFO that holds IDs of kernels that are currently active
/// Empty means no kernel is currently active
/// The index always points to the slot of the next entry
uint32_t stackIndex = 0;
uint64_t seenKernels[STACK_SIZE];
/// Flag initiating the profiler
bool instanceActive = false;

//extern "C"
//{
    void ReadKernelFile()
    {
        char* kfName = getenv("KERNEL_FILE");
        if( !kfName )
        {
            cout << "Critical: KERNEL_FILE variable must be set!" << endl;
            exit(EXIT_FAILURE);
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
            cout << "Critical: Couldn't open kernel file: " + string(kfName));
            cout << e.what() << endl;;
            exit(EXIT_FAILURE);
        }
        for (const auto &bbid : j.items())
        {
            if (j[bbid.key()].find("Kernels") != j[bbid.key()].end())
            {
                for( const auto& kid : j[bbid.key()]["Kernels"].items() )
                {
                    auto newKernel = KernelInstance(stoi(kid.key()));
                    // need blocks, entrances, exits, children and parents
                    newKernel.blocks.insert(j[bbid.key()]["Kernels"][kid.key()]["Blocks"].begin(), j[bbid.key()]["Kernels"][kid.key()]["Blocks"].end());
                }
                kernels.insert(newKernel);
            }
        }
    }

    void InstanceInit(uint64_t a)
    {
        instanceActive = true;
        InstanceIncrement(a);
    }

    void InstanceDestroy()
    {
        // do nothing for now
    }

    void InstanceIncrement(uint64_t a)
    {
        if( !instanceActive )
        {
            return;
        }
        // first step, acquire all kernels who have this block in them
        // we want the kernel instance that contains this block "organically", not through its children
        std::set<KernelInstance> liveKernels;
        for( const auto& kern : kernels )
        {
            if( kern.blocks.find((uint32_t)a) != kern.blocks.end() )
            {
                liveKernels.insert(kern.)
            }
        }
        // second step, pick out the kernels that are parents of the current live kernel
        KernelInstance activeKernel;
        for( const auto& kern : liveKernels )
        {
            if( !kern.children.empty() )
            {
                bool foundChild = false;
                for( const auto& child : kern.children )
                {
                    if( liveKernels.find(child) != liveKernels.end() )
                    {
                        // this is a parent kernel, move on
                        foundChild = true;
                        break;
                    }
                }
                if( foundChild )
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
        // third, identify if we just entered or exited the kernel and modify the stack accordingly
        auto foundBlock = false;
        for( const auto& block : activeKernel.entrances )
        {
            if( block == (uint32_t)a )
            {
                pushStack(activeKernel.ID);
                foundBlock = true;
                break;
            }
        }
        if( foundBlock )
        {
            return;
        }
        for( const auto& block : activeKernel.exits )
        {
            if( block == (uint32_t)a )
            {
                popStack(activeKernel.ID);
            }
        }
    }

    void pushStack(int KID)
    {
        if( stackIndex < STACK_SIZE )
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
        if( stackIndex > 0 )
        {
            stackIndex--;
            return seenKernels[stackIndex + 1];
        }
        return seenKernels[0];
    }
//}

/*

// stack for storing labels
char *labelStack[STACK_SIZE];
uint32_t stackCount = 0;

void pushStack(char *newLabel)
{
    stackCount++;
    if (stackCount < STACK_SIZE)
    {
        labelStack[stackCount] = newLabel;
    }
}

char *popStack()
{
    char *pop = labelStack[stackCount];
    stackCount--;
    return pop;
}

// counter that is used to index the circular buffer. The index (increment%MARKOV_ORDER) always point to the oldest entry in the buffer (after it is fully initialized)
uint64_t blocksSeen = 0;
// Indicates which block was the caller of the current context
int64_t openIndicator = -1;
// holds the count of all blocks in the bitcode source file
uint64_t totalBlocks;
// Circular buffer of the previous MARKOV_ORDER blocks seen
uint64_t b[MARKOV_ORDER];
// Flag indicating whether the program is actively being profiled
bool markovActive = false;
// Hash table for the edges of the control flow graph
__TA_HashTable *edgeHashTable;
// Hash table for the labels of each basic block
__TA_HashTable *labelHashTable;
// Hash table for the caller-callee hash table
__TA_HashTable *callerHashTable;
// Global structure that is used to increment the last seen edge in the hash table
__TA_element nextEdge;
__TA_element nextLabel;
__TA_element nextCallee;

extern "C"
{
    void MarkovInit(uint64_t blockCount, uint64_t ID)
    {
        // edge circular buffer initialization
        b[0] = ID;

        // edge hash table
        edgeHashTable = (__TA_HashTable *)malloc(sizeof(__TA_HashTable));
        edgeHashTable->size = (uint32_t)(ceil(log((double)blockCount) / log(2.0)));
        edgeHashTable->getFullSize = __TA_getFullSize;
        edgeHashTable->array = (__TA_arrayElem *)calloc(edgeHashTable->getFullSize(edgeHashTable), sizeof(__TA_arrayElem));
        // label hash table
        labelHashTable = (__TA_HashTable *)malloc(sizeof(__TA_HashTable));
        labelHashTable->size = (uint32_t)(ceil(log((double)blockCount) / log(2.0)));
        labelHashTable->getFullSize = __TA_getFullSize;
        labelHashTable->array = (__TA_arrayElem *)calloc(labelHashTable->getFullSize(labelHashTable), sizeof(__TA_arrayElem));
        // caller hash table
        callerHashTable = (__TA_HashTable *)malloc(sizeof(__TA_HashTable));
        callerHashTable->size = (uint32_t)(ceil(log((double)blockCount) / log(2.0)));
        callerHashTable->getFullSize = __TA_getFullSize;
        callerHashTable->array = (__TA_arrayElem *)calloc(callerHashTable->getFullSize(callerHashTable), sizeof(__TA_arrayElem));

        totalBlocks = blockCount;
        markovActive = true;
    }
    void MarkovDestroy()
    {
        // print profile bin file
        __TA_WriteEdgeHashTable(edgeHashTable, (uint32_t)totalBlocks);
        free(edgeHashTable->array);
        free(edgeHashTable);

        // construct BlockInfo json output
        json labelMap;
        for (uint32_t i = 0; i < labelHashTable->getFullSize(labelHashTable); i++)
        {
            for (uint32_t j = 0; j < labelHashTable->array[i].popCount; j++)
            {
                auto entry = labelHashTable->array[i].tuple[j];
                char label[100];
                sprintf(label, "%d", entry.label.blocks[0]);
                labelMap[string(label)]["Labels"] = map<string, uint64_t>();
            }
        }
        for (uint32_t i = 0; i < callerHashTable->getFullSize(callerHashTable); i++)
        {
            for (uint32_t j = 0; j < callerHashTable->array[i].popCount; j++)
            {
                auto entry = callerHashTable->array[i].tuple[j];
                char label[100];
                sprintf(label, "%d", entry.label.blocks[0]);
                labelMap[string(label)]["BlockCallers"] = vector<string>();
            }
        }
        for (uint32_t i = 0; i < labelHashTable->getFullSize(labelHashTable); i++)
        {
            for (uint32_t j = 0; j < labelHashTable->array[i].popCount; j++)
            {
                auto entry = labelHashTable->array[i].tuple[j];
                char label[100];
                sprintf(label, "%d", entry.label.blocks[0]);
                labelMap[string(label)]["Labels"][string(entry.label.label)] = entry.label.frequency;
            }
        }
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

        // free everything
        file.close();
        free(labelHashTable->array);
        free(labelHashTable);
        free(callerHashTable->array);
        free(callerHashTable);
        markovActive = false;
    }
    void MarkovIncrement(uint64_t a)
    {
        if (!markovActive)
        {
            return;
        }
        // edge hash table
        for (uint8_t i = 0; i < MARKOV_ORDER; i++)
        {
            // nextEdge.blocks must always be in chronological order. Thus we start from the beginning with that index (which is what the offset calculation is for)
            int index = (blocksSeen + i) % MARKOV_ORDER;
            nextEdge.edge.blocks[i] = (uint32_t)b[index];
        }
        nextEdge.edge.blocks[MARKOV_ORDER] = (uint32_t)a;
        b[blocksSeen % MARKOV_ORDER] = a;
        blocksSeen++;
        // we need to wait until the first MARKOV_ORDER blocks have been seen (MarkovInit records the first block, thus after MARKOV_ORDER blocks have been seen, we have our first edge)
        if (blocksSeen < MARKOV_ORDER)
        {
            return;
        }
        while (__TA_HashTable_increment(edgeHashTable, &nextEdge))
        {
#ifdef DEBUG
            cout << "Resolving clash in edge table" << endl;
#endif
            __TA_resolveClash(edgeHashTable, edgeHashTable->size + 1);
        }

        // label hash table
        if (stackCount > 0)
        {
            nextLabel.label.blocks[0] = (uint32_t)a;
            // here we use the LSB of the label pointer to help hash more effectively
            nextLabel.label.blocks[1] = (uint64_t)labelStack[stackCount] & 0xFFFF;
            nextLabel.label.label = labelStack[stackCount];
            while (__TA_HashTable_increment(labelHashTable, &nextLabel))
            {
#ifdef DEBUG
                cout << "Resolving clash in label table" << endl;
#endif
                __TA_resolveClash(labelHashTable, labelHashTable->size + 1);
            }
        }

        // caller hash table
        // mark our block caller, if necessary
        if (openIndicator != -1)
        {
            nextCallee.callee.blocks[0] = (uint32_t)openIndicator;
            nextCallee.callee.blocks[1] = (uint32_t)a;
            while (__TA_HashTable_increment(callerHashTable, &nextCallee))
            {
#ifdef DEBUG
                cout << "Resolving clash in caller table" << endl;
#endif
                __TA_resolveClash(callerHashTable, callerHashTable->size + 1);
            }
        }
        openIndicator = (int64_t)a;
    }
    void MarkovExit()
    {
        openIndicator = -1;
    }
    void TraceAtlasMarkovKernelEnter(char *label)
    {
        pushStack(label);
    }
    void TraceAtlasMarkovKernelExit()
    {
        popStack();
    }
    */
}