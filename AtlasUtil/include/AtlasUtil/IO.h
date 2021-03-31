#pragma once
#include "AtlasUtil/Graph/Graph.h"
#include "AtlasUtil/Print.h"
#include "llvm/Analysis/CallGraph.h"
#include <fstream>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <vector>

static llvm::LLVMContext context;
static llvm::SMDiagnostic smerror;

inline std::unique_ptr<llvm::Module> ReadBitcode(const std::string &InputFilename)
{
    std::unique_ptr<llvm::Module> SourceBitcode = parseIRFile(InputFilename, smerror, context);
    if (SourceBitcode.get() == nullptr)
    {
        spdlog::critical("Failed to open bitcode file: " + InputFilename);
    }
    return SourceBitcode;
}

inline std::vector<std::unique_ptr<llvm::Module>> LoadBitcodes(const std::vector<std::string> &paths)
{
    std::vector<std::unique_ptr<llvm::Module>> result;
    for (const auto &path : paths)
    {
        result.push_back(ReadBitcode(path));
    }
    return result;
}

inline std::map<int64_t, int64_t> ReadBlockInfo(std::string &BlockInfo)
{
    std::map<int64_t, int64_t> blockCallers;
    std::ifstream inputJson;
    nlohmann::json j;
    try
    {
        inputJson.open(BlockInfo);
        inputJson >> j;
        inputJson.close();
    }
    catch (std::exception &e)
    {
        spdlog::error("Couldn't open BlockInfo json file: " + BlockInfo);
        spdlog::error(e.what());
        return blockCallers;
    }
    for (const auto &bbid : j.items())
    {
        auto vec = j[bbid.key()]["BlockCallers"].get<std::vector<int64_t>>();
        if (vec.size() == 1)
        {
            blockCallers[stol(bbid.key())] = *vec.begin();
        }
        else if (vec.size() > 1)
        {
            throw AtlasException("Found more than one entry in a blockCaller key!");
        }
    }
    return blockCallers;
}

inline std::map<int64_t, std::map<std::string, int64_t>> ReadBlockLabels(std::string &BlockInfo)
{
    std::map<int64_t, std::map<std::string, int64_t>> blockLabels;
    std::ifstream inputJson;
    nlohmann::json j;
    try
    {
        inputJson.open(BlockInfo);
        inputJson >> j;
        inputJson.close();
    }
    catch (std::exception &e)
    {
        spdlog::error("Couldn't open BlockInfo json file: " + BlockInfo);
        spdlog::error(e.what());
        return blockLabels;
    }
    for (const auto &bbid : j.items())
    {
        if (j[bbid.key()].find("Labels") != j[bbid.key()].end())
        {
            auto labelCounts = j[bbid.key()]["Labels"].get<std::map<std::string, int64_t>>();
            blockLabels[stol(bbid.key())] = labelCounts;
        }
    }
    return blockLabels;
}

/* This hasn't worked yet because the constructors are undefined symbols at link time for this include file
void ReadBIN(std::set<TraceAtlas::Cartographer::GraphNode *, TraceAtlas::Cartographer::p_GNCompare> &nodes, const std::string &filename, bool print = false)
{
    std::fstream inputFile;
    inputFile.open(filename, std::ios::in | std::ios::binary);
    while (inputFile.peek() != EOF)
    {
        // New block description: BBID,#ofNeighbors (16 bytes per neighbor)
        uint64_t key;
        inputFile.readsome((char *)&key, sizeof(uint64_t));
        TraceAtlas::Cartographer::GraphNode currentNode;
        if (nodes.find(key) == nodes.end())
        {
            currentNode = TraceAtlas::Cartographer::GraphNode(key);
            // when reading the trace file, NID and blockID are 1to1
            currentNode.blocks[(int64_t)key] = (int64_t)key;
        }
        else
        {
            spdlog::error("Found a BBID that already existed in the graph!");
        }
        // the instance count of the edge
        uint64_t count;
        // for summing the total count of the neighbors
        uint64_t sum = 0;
        inputFile.readsome((char *)&count, sizeof(uint64_t));
        for (uint64_t i = 0; i < count; i++)
        {
            uint64_t k2;
            inputFile.readsome((char *)&k2, sizeof(uint64_t));
            uint64_t val;
            inputFile.readsome((char *)&val, sizeof(uint64_t));
            if (val > 0)
            {
                sum += val;
                currentNode.neighbors[k2] = std::pair(val, 0.0);
            }
        }
        for (auto &key : currentNode.neighbors)
        {
            key.second.second = (double)key.second.first / (double)sum;
        }
        nodes.insert(new TraceAtlas::Cartographer::GraphNode(currentNode));
    }
    inputFile.close();

    // now fill in all the predecessor nodes
    for (auto &node : nodes)
    {
        for (const auto &neighbor : node->neighbors)
        {
            auto successorNode = nodes.find(neighbor.first);
            if (successorNode != nodes.end())
            {
                (*successorNode)->predecessors.insert(node->NID);
            }
            // the trace doesn't include the terminating block of the program (because it has no edges leading from it)
            // But this creates a problem when defining kernel exits, so look for the node who has a neighbor that is not in the set already and add that neighbor (with correct predecessor)
            else
            {
                // we likely found the terminating block, so add the block and assign the current node to be its predecessor
                auto programTerminator = TraceAtlas::Cartographer::GraphNode(neighbor.first);
                programTerminator.blocks[(int64_t)programTerminator.NID] = (int64_t)programTerminator.NID;
                programTerminator.predecessors.insert(node->NID);
                nodes.insert(new TraceAtlas::Cartographer::GraphNode(programTerminator));
            }
        }
    }
    if( print )
    {
        PrintGraph(nodes);
    }
}
*/
inline Graph<uint64_t> LoadBIN(const std::string &path)
{
    spdlog::trace("Loading bin file: {0}", path);
    Graph<uint64_t> result;
    std::fstream inputFile;
    inputFile.open(path, std::ios::in | std::ios::binary);
    while (inputFile.peek() != EOF)
    {
        uint64_t key;
        inputFile.readsome((char *)&key, sizeof(uint64_t));
        uint64_t count;
        inputFile.readsome((char *)&count, sizeof(uint64_t));
        for (uint64_t i = 0; i < count; i++)
        {
            uint64_t k2;
            inputFile.readsome((char *)&k2, sizeof(uint64_t));
            uint64_t val;
            inputFile.readsome((char *)&val, sizeof(uint64_t));
            result.WeightMatrix[key][k2] = val;
        }
    }
    inputFile.close();
    //temporary, needs modification for the future
    for (uint64_t i = 0; i < result.WeightMatrix.size(); i++)
    {
        result.IndexAlias[i].push_back(i);
        result.LocationAlias[i] = i;
    }
    //for (uint64_t i = 0; i < result.WeightMatrix.size(); i++)
    //{
    for (const auto &key : result.WeightMatrix)
    {
        // Has an alloc problem (std::bad_alloc exception).
        // this broke in FFTW/2d_512. When building this matrix, the array is so large the spade node runs out of memory (observed on top to demand greater than 220GB)
        // The [] operator automatically allocates a new place if the key doesn't exist. Therefore just iterate through the keys
        //for (uint64_t j = 0; j < result.WeightMatrix[i].size(); j++)
        //{
        for (const auto &sk : key.second)
        {
            if (sk.second != 0)
            {
                result.NeighborMap[key.first].insert(sk.first);
            }
        }
    }
    return result;
}

inline llvm::CallGraph getCallGraph(llvm::Module *mod, std::map<int64_t, int64_t> &blockCallers, std::map<llvm::BasicBlock *, llvm::Function *> &BlockToFPtr, std::map<int64_t, llvm::BasicBlock *> &IDToBlock)
{
    llvm::CallGraph CG(*mod);
    // Add function pointers
    for (auto &f : *mod)
    {
        for (auto bb = f.begin(); bb != f.end(); bb++)
        {
            for (auto it = bb->begin(); it != bb->end(); it++)
            {
                if (auto CI = llvm::dyn_cast<llvm::CallInst>(it))
                {
                    auto callee = CI->getCalledFunction();
                    if (callee == nullptr)
                    {
                        // try to find a block caller entry for this function, if it's not there we have to move on
                        auto BBID = GetBlockID(llvm::cast<llvm::BasicBlock>(bb));
                        if (blockCallers.find(BBID) != blockCallers.end())
                        {
                            auto calleeID = blockCallers[BBID]; // should only be 1 element long, because of basic block splitting
                            auto calleeBlock = IDToBlock[calleeID];
                            if (calleeBlock != nullptr)
                            {
                                auto parentNode = CG.getOrInsertFunction(bb->getParent());
                                auto childNode = CG.getOrInsertFunction(calleeBlock->getParent());
                                parentNode->addCalledFunction(CI, childNode);
                                BlockToFPtr[llvm::cast<llvm::BasicBlock>(bb)] = calleeBlock->getParent();
                            }
                        }
                        else
                        {
                            spdlog::warn("BlockCallers did not contain an entry for the indirect call in BBID " + std::to_string(BBID));
                        }
                    }
                }
            }
        }
    }
    return CG;
}