#pragma once
#include "AtlasUtil/Graph/Graph.h"
#include "AtlasUtil/Print.h"
#include <fstream>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <memory>
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

inline Graph<uint64_t> LoadCSV(const std::string &path)
{
    Graph<uint64_t> result;
    std::fstream inputFile;
    inputFile.open(path, std::ios::in);
    std::string ln;
    while (getline(inputFile, ln))
    {
        std::vector<uint64_t> lVec;
        std::stringstream lnStream(ln);
        std::string sub;
        while (getline(lnStream, sub, ','))
        {
            lVec.push_back(std::stoull(sub));
        }
        result.WeightMatrix.push_back(lVec);
    }
    inputFile.close();
    //temporary, needs modification for the future
    for (uint64_t i = 0; i < result.WeightMatrix.size(); i++)
    {
        result.IndexAlias[i].push_back(i);
        result.LocationAlias[i] = i;
    }
    return result;
}