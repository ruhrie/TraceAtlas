#pragma once
#include "AtlasUtil/Annotate.h"
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
        if (j[bbid.key()].find("BlockCallers") != j[bbid.key()].end())
        {
            auto vec = j[bbid.key()]["BlockCallers"].get<std::vector<int64_t>>();
            if (vec.size() == 1)
            {
                blockCallers[stol(bbid.key())] = *vec.begin();
            }
            else if (vec.size() > 1)
            {
                //throw AtlasException("Found more than one entry in a blockCaller key!");
            }
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