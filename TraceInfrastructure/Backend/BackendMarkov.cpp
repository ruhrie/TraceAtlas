#include "Backend/DashHashTable.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <unordered_map>
#include <vector>

using namespace std;
using json = nlohmann::json;

/*class dict
{
public:
    unordered_map<uint64_t, unordered_map<uint64_t, uint64_t>> base;
    ~dict()
    {
        char *tfn = getenv("MARKOV_FILE");
        string fileName;
        if (tfn != nullptr)
        {
            fileName = tfn;
        }
        else
        {
            fileName = "markov.bin";
        }
        ofstream fp(fileName, std::ios::out | std::ios::binary);
        for (auto addr1 : base)
        {
            // first, write source node ID
            // second, write # of outgoing edges (length of non-zero entries)
            // third, write the sink node ID
            // fourth, write the frequency count of the edge
            fp.write((const char *)&addr1.first, sizeof(uint64_t));
            uint64_t length = addr1.second.size();
            fp.write((const char *)&length, sizeof(std::size_t)); // BW are you sure this should be size_t and not uint64_t?
            for (auto addr2 : addr1.second)
            {
                fp.write((const char *)&addr2.first, sizeof(uint64_t));
                fp.write((const char *)&addr2.second, sizeof(std::size_t)); // same with this
            }
        }
        fp.close();
    }
};*/

//map<string, set<uint64_t>> blockCallers;

/*struct labelMap
{
    map<string, map<string, uint64_t>> blockLabels;
    ~labelMap()
    {
        json labelMap;
        for (const auto &bbid : blockLabels)
        {
            labelMap[bbid.first]["Labels"] = bbid.second;
        }
        for (const auto &bbid : blockCallers)
        {
            labelMap[bbid.first]["BlockCallers"] = bbid.second;
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
        file.close();
    }
};*/

// Indicates which block was the caller of the current context
long openIndicator = -1;
uint64_t totalBlocks;
// Circular buffer of the previous MARKOV_ORDER blocks seen
uint64_t b[MARKOV_ORDER];
// counter that is used to index the circular buffer. The index (increment%MARKOV_ORDER) always point to the oldest entry in the buffer (after it is fully initialized)
uint8_t increment;
// Flag indicating whether the program is actively being profiled
bool markovActive = false;
// Hash table of the profiler
__TA_HashTable *hashTable;
// Global structure that is used to increment the last seen edge in the hash table
__TA_element nextEdge;
//dict TraceAtlasMarkovMap;
//labelMap TraceAtlasLabelMap;
//vector<char *> labelList;

extern "C"
{
    void MarkovInit(uint64_t blockCount, uint64_t ID)
    {
        increment = 0;
        b[0] = ID;
        hashTable = (__TA_HashTable *)malloc(sizeof(__TA_HashTable));
        hashTable->size = (uint32_t)(ceil(log((double)blockCount) / log(2.0)));
        hashTable->getFullSize = __TA_getFullSize;
        hashTable->array = (__TA_arrayElem *)malloc(hashTable->getFullSize(hashTable) * sizeof(__TA_arrayElem));
        totalBlocks = blockCount;
        markovActive = true;
    }
    void MarkovDestroy()
    {
        __TA_WriteHashTable(hashTable, (uint32_t)totalBlocks);
        free(hashTable->array);
        free(hashTable);
        // just write an output BlockInfo file for now to get past file checked in automation tool
        char *blockFile = getenv("BLOCK_FILE");
        FILE *f;
        if (blockFile)
        {
            f = fopen(blockFile, "w");
        }
        else
        {
            f = fopen("BlockInfo.json", "w");
        }
        fclose(f);
        markovActive = false;
    }
    void MarkovIncrement(uint64_t a)
    {
        if (markovActive)
        {
            for (uint8_t i = 0; i < MARKOV_ORDER; i++)
            {
                // nextEdge.blocks must always be in chronological order. Thus we start from the beginning with that index (which is what the offset calculation is for)
                nextEdge.edge.blocks[i] = (uint32_t)b[(increment + i) % MARKOV_ORDER];
            }
            nextEdge.edge.blocks[MARKOV_ORDER] = (uint32_t)a;
            while (__TA_HashTable_increment(hashTable, &nextEdge))
            {
                __TA_resolveClash(hashTable);
            }
            b[increment % MARKOV_ORDER] = a;
            increment++;
            /*if (!labelList.empty())
            {
                string labelName(labelList.back());
                if (TraceAtlasLabelMap.blockLabels.find(to_string(a)) == TraceAtlasLabelMap.blockLabels.end())
                {
                    TraceAtlasLabelMap.blockLabels[to_string(a)] = map<string, uint64_t>();
                    blockCallers[to_string(a)] = set<uint64_t>();
                }
                if (TraceAtlasLabelMap.blockLabels[to_string(a)].find(labelName) == TraceAtlasLabelMap.blockLabels[to_string(a)].end())
                {
                    TraceAtlasLabelMap.blockLabels[to_string(a)][labelName] = 0;
                }
                TraceAtlasLabelMap.blockLabels[to_string(a)][labelName]++;
            }
            // mark our block caller, if necessary
            if (openIndicator != -1)
            {
                blockCallers[to_string(openIndicator)].insert(a);
            }
            openIndicator = (long)a;*/
        }
    }
    void MarkovExit()
    {
        openIndicator = -1;
    }
    void TraceAtlasMarkovKernelEnter(char *label)
    {
        //labelList.push_back(label);
    }
    void TraceAtlasMarkovKernelExit()
    {
        //labelList.pop_back();
    }
}