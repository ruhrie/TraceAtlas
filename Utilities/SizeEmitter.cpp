#include <fstream>
#include <iostream>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
using namespace llvm;
using json = nlohmann::json;

cl::opt<std::string> InputFilename("i", cl::desc("Specify input filename"), cl::value_desc("filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output filename"), cl::value_desc("filename"), cl::Required);

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    std::map<uint64_t, std::vector<int>> storeSizes;
    std::map<uint64_t, std::vector<int>> loadSizes;
    LLVMContext context;
    SMDiagnostic smerror;
    std::unique_ptr<Module> mptr = parseIRFile(InputFilename, smerror, context);
    Module *m = mptr.get();

    DataLayout dl(m);

    //std::cout << m->getName();
    //std::cout << m->size();

    for (auto &mi : *m)
    {
        for (auto fi = mi.begin(); fi != mi.end(); fi++)
        {
            auto *BB = cast<BasicBlock>(fi);
            std::string name = BB->getName();
            uint64_t id = std::stoul(name.substr(7));
            for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
            {
                auto *CI = cast<Instruction>(BI);
                if (auto *li = dyn_cast<LoadInst>(CI))
                {
                    Type *type = li->getType();
                    int size = dl.getTypeAllocSize(type);
                    loadSizes[id].push_back(size);
                }
                else if (auto *si = dyn_cast<StoreInst>(CI))
                {
                    Type *type = si->getValueOperand()->getType();
                    int size = dl.getTypeAllocSize(type);
                    storeSizes[id].push_back(size);
                }
            }
        }
    }

    std::map<std::string, std::map<uint64_t, std::vector<int>>> finalMap;
    finalMap["Stores"] = storeSizes;
    finalMap["Loads"] = loadSizes;
    json j_map(finalMap);

    std::ofstream file;
    file.open(OutputFilename);
    file << j_map;
    file.close();

    return 0;
}