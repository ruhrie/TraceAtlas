#pragma once
#include <fstream>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

inline std::unique_ptr<llvm::Module> LoadBitcode(const std::string &path)
{
    llvm::LLVMContext context;
    llvm::SMDiagnostic smerror;
    auto sourceBitcode = llvm::parseIRFile(path, smerror, context);
    return sourceBitcode;
}

inline std::vector<std::vector<uint64_t>> LoadCSV(const std::string &path)
{
    std::vector<std::vector<uint64_t>> result;
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
        result.push_back(lVec);
    }
    inputFile.close();
    return result;
}