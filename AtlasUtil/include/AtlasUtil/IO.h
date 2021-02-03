#pragma once
#include "AtlasUtil/Graph/Graph.h"
#include "AtlasUtil/Print.h"
#include <fstream>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <vector>

static llvm::LLVMContext context;
static llvm::SMDiagnostic smerror;

inline std::unique_ptr<llvm::Module> LoadBitcode(const std::string &path)
{
    auto sourceBitcode = llvm::parseIRFile(path, smerror, context);
    return sourceBitcode;
}

inline std::vector<std::unique_ptr<llvm::Module>> LoadBitcodes(const std::vector<std::string> &paths)
{
    std::vector<std::unique_ptr<llvm::Module>> result;
    for (const auto &path : paths)
    {
        result.push_back(LoadBitcode(path));
    }
    return result;
}

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