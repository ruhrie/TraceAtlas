#include "AtlasUtil/Format.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <vector>

using namespace llvm;
using json = nlohmann::json;
using namespace std;

cl::opt<std::string> InputFilename("i", cl::desc("Specify input bitcode"), cl::value_desc("bitcode filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"));
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);

static int UID = 0;
static int valueId = 0;

string getName()
{
    string name = "v_" + to_string(valueId++);
    return name;
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    LLVMContext context;
    SMDiagnostic smerror;
    std::unique_ptr<Module> mptr = parseIRFile(InputFilename, smerror, context);

    // Annotate its bitcodes and values
    CleanModule(mptr.get());
    Format(mptr.get());

    map<int64_t, BasicBlock *> IDToBlock;
    map<int64_t, Value *> IDToValue;
    InitializeIDMaps(mptr.get(), IDToBlock, IDToValue);

    ifstream inputJson;
    nlohmann::json j;
    try
    {
        inputJson.open(KernelFilename);
        inputJson >> j;
        inputJson.close();
    }
    catch (exception &e)
    {
        spdlog::error("Couldn't open input json file: " + KernelFilename);
        spdlog::error(e.what());
        return EXIT_FAILURE;
    }

    map<string, std::set<uint64_t>> blockHash;
    map<string, uint64_t> kernelHash;
    hash<string> hasher;
    for (auto &[key, value] : j["Kernels"].items())
    {
        vector<int> blocks = value["Blocks"];
        std::sort(blocks.begin(), blocks.end());
        vector<string> blockStrings;
        for (int block : blocks)
        {
            BasicBlock *toConvert = IDToBlock[block];
            valueId = 0;
            string blockStr;
            vector<Value *> namedVals;
            for (BasicBlock::iterator BI = toConvert->begin(), BE = toConvert->end(); BI != BE; ++BI)
            {
                auto *inst = cast<Instruction>(BI);
                uint32_t ops = inst->getNumOperands();
                for (int i = 0; i < ops; i++)
                {
                    Value *op = inst->getOperand(i);
                    op->setName(getName());
                    namedVals.push_back(op);
                }
                if (std::find(namedVals.begin(), namedVals.end(), inst) == namedVals.end())
                {
                    inst->setName(getName());
                    namedVals.push_back(inst);
                }
                std::string str;
                llvm::raw_string_ostream rso(str);
                inst->print(rso);
                blockStr += str + "\n";
            }
            for (Value *v : namedVals)
            {
                v->setName("");
            }
            namedVals.clear();
            blockStr += "\n";
            blockStrings.push_back(blockStr);

            std::sort(blockStr.begin(), blockStr.end());
            blockHash[key].insert(hasher(blockStr));
        }

        std::sort(blockStrings.begin(), blockStrings.begin());

        string toHash;
        for (const auto &str : blockStrings)
        {
            toHash += str + "\n";
        }

        uint64_t hashed = hasher(toHash);
        kernelHash[key] = hashed;
    }

    json j_map;
    for (const auto &key : kernelHash)
    {
        j_map[key.first]["Kernel"] = key.second;
        j_map[key.first]["Blocks"] = vector<uint64_t>(blockHash[key.first].begin(), blockHash[key.first].end());
    }
    if (!OutputFilename.empty())
    {
        std::ofstream file;
        file.open(OutputFilename);
        file << j_map;
        file.close();
    }
    else
    {
        cout << j_map << "\n";
    }

    return 0;
}