#include "AtlasUtil/Format.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include <fstream>
#include <iomanip>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/spdlog.h>

using namespace llvm;
using json = nlohmann::json;
using namespace std;

cl::opt<std::string> InputFilename("i", cl::desc("Input bitcode. Must be compiled with maximum debug symbols to optimize the result of this tool."), cl::value_desc("input.bc"), cl::Required);
cl::opt<std::string> KernelFilename("k", cl::desc("Input kernel .json file"), cl::value_desc("kernel.json"), cl::Required);
cl::opt<std::string> MapFileName("o", cl::desc("Output map file"), cl::value_desc("kernel filename"), cl::init("kernelMap.json"));

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
    std::unique_ptr<Module> SourceBitcode = parseIRFile(InputFilename, smerror, context);

    // Don't clean the debug info, but format it like the rest of the tools
    Format(SourceBitcode.get());

    map<int64_t, BasicBlock *> IDToBlock;
    map<int64_t, Value *> IDToValue;
    InitializeIDMaps(SourceBitcode.get(), IDToBlock, IDToValue);

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

    json kernelMap;
    for (auto &[key, value] : j["Kernels"].items())
    {
        map<string, set<int>> sourceLines;
        for (const auto &BID : value["Blocks"])
        {
            auto block = IDToBlock[BID];
            for (auto &bb : *block) //->begin(); bb != block->end(); bb++)
            {
                if (bb.hasMetadata())
                {
                    const auto &LOC = bb.getDebugLoc();
                    if (LOC.getAsMDNode() != nullptr)
                    {
                        if (auto scope = dyn_cast<DIScope>(LOC.getScope()))
                        {
                            string dir = scope->getFile()->getDirectory();
                            dir.append("/");
                            dir.append(scope->getFile()->getFilename());
                            sourceLines[dir].insert(LOC.getLine());
                        }
                    }
                }
            }
        }
        kernelMap[key] = sourceLines;
    }
    std::ofstream file;
    file.open(MapFileName);
    file << setw(4) << kernelMap;
    file.close();
    return 0;
}