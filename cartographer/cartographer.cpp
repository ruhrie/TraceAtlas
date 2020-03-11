#include "AtlasUtil/Annotate.h"
#include "EncodeDetect.h"
#include "EncodeExtract.h"
#include "Rectifier.h"
#include "Smoothing.h"
#include "profile.h"
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <string>
#include <tuple>

using namespace std;
using namespace llvm;

bool noProgressBar;
bool blocksLabeled = false;
map<int, set<string>> blockLabelMap;
map<int, BasicBlock *> blockMap;

llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
llvm::cl::opt<float> threshold("t", cl::desc("The threshold of block grouping required to complete a kernel."), llvm::cl::value_desc("float"), llvm::cl::init(0.9));
llvm::cl::opt<int> hotThreshold("ht", cl::desc("The minimum instance count for a basic block to be a seed."), llvm::cl::init(512));
llvm::cl::opt<string> kernelFile("k", llvm::cl::desc("Specify output json name"), llvm::cl::value_desc("kernel filename"), llvm::cl::init("kernel.json"));
llvm::cl::opt<string> profileFile("p", llvm::cl::desc("Specify profile json name"), llvm::cl::value_desc("profile filename"));
llvm::cl::opt<string> bitcodeFile("b", llvm::cl::desc("Specify bitcode name"), llvm::cl::value_desc("bitcode filename"), llvm::cl::Required);
llvm::cl::opt<bool> label("L", llvm::cl::desc("ExportLabel"), llvm::cl::value_desc("Export library label"), cl::init(false));
llvm::cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));
cl::opt<int> LogLevel("v", cl::desc("Logging level"), cl::value_desc("logging level"), cl::init(4));
cl::opt<string> LogFile("l", cl::desc("Specify log filename"), cl::value_desc("log file"));
int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    noProgressBar = noBar;

    if (!LogFile.empty())
    {
        auto file_logger = spdlog::basic_logger_mt("cartographer_logger", LogFile);
        spdlog::set_default_logger(file_logger);
    }

    switch (LogLevel)
    {
        case 0:
        {
            spdlog::set_level(spdlog::level::off);
            break;
        }
        case 1:
        {
            spdlog::set_level(spdlog::level::critical);
            break;
        }
        case 2:
        {
            spdlog::set_level(spdlog::level::err);
            break;
        }
        case 3:
        {
            spdlog::set_level(spdlog::level::warn);
            break;
        }
        case 4:
        {
            spdlog::set_level(spdlog::level::info);
            break;
        }
        case 5:
        {
            spdlog::set_level(spdlog::level::debug);
        }
        case 6:
        {
            spdlog::set_level(spdlog::level::trace);
            break;
        }
        default:
        {
            spdlog::warn("Invalid logging level: " + to_string(LogLevel));
        }
    }

    LLVMContext context;
    SMDiagnostic smerror;
    unique_ptr<Module> sourceBitcode;
    try
    {
        sourceBitcode = parseIRFile(bitcodeFile, smerror, context);
    }
    catch (exception e)
    {
        spdlog::critical("Failed to open bitcode file: " + bitcodeFile);
        return EXIT_FAILURE;
    }

    Module *M = sourceBitcode.get();
    Annotate(M);

    //build the blockMap
    for (auto mi = M->begin(); mi != M->end(); mi++)
    {
        for (auto fi = mi->begin(); fi != mi->end(); fi++)
        {
            BasicBlock *bb = cast<BasicBlock>(fi);
            int64_t id = GetBlockID(bb);
            blockMap[id] = bb;
        }
    }

    try
    {
        std::set<std::set<int>> type1Kernels;
        spdlog::info("Started analysis");
        type1Kernels = DetectKernels(inputTrace, threshold, hotThreshold);
        spdlog::info("Detected " + to_string(type1Kernels.size()) + " type 1 kernels");
        auto type2Kernels = ExtractKernels(inputTrace, type1Kernels, M);
        spdlog::info("Detected " + to_string(type2Kernels.size()) + " type 2 kernels");
        auto type25Kernels = ExtractKernels(inputTrace, type2Kernels, M);
        spdlog::info("Detected " + to_string(type25Kernels.size()) + " type 2.5 kernels");
        set<set<int>> type3Kernels = SmoothKernel(type25Kernels, M);
        spdlog::info("Detected " + to_string(type3Kernels.size()) + " type 3 kernels");
        auto type4Kernels = RectifyKernel(type3Kernels, M);
        spdlog::info("Detected " + to_string(type4Kernels.size()) + " type 4 kernels");

        map<int, set<int>> finalResult;
        int j = 0;
        for (auto set : type4Kernels)
        {
            if (!set.empty())
            {
                finalResult[j++] = set;
            }
        }

        vector<int> validBlocks;
        for (auto &[block, count] : blockCount)
        {
            if (count != 0)
            {
                validBlocks.push_back(block);
            }
        }

        nlohmann::json outputJson;
        for (auto key : finalResult)
        {
            if (label)
            {
                string strLabel = "";
                bool first = true;
                int i = 0;
                set<string> labels;
                for (auto block : key.second)
                {
                    labels.insert(blockLabelMap[block].begin(), blockLabelMap[block].end());
                }
                for (auto entry : labels)
                {
                    if (entry.empty())
                    {
                        continue;
                    }
                    if (first)
                    {
                        first = false;
                    }
                    else
                    {
                        strLabel += ";";
                    }
                    strLabel += entry;
                }
                outputJson["Kernels"][to_string(key.first)]["Blocks"] = key.second;
                outputJson["Kernels"][to_string(key.first)]["Label"] = strLabel;
            }
            else
            {
                outputJson["Kernels"][to_string(key.first)]["Blocks"] = key.second;
            }
        }
        outputJson["ValidBlocks"] = validBlocks;
        ofstream oStream(kernelFile);
        oStream << outputJson;
        oStream.close();
        if (!profileFile.empty())
        {
            nlohmann::json prof = ProfileKernels(finalResult, sourceBitcode.get());
            ofstream pStream(profileFile);
            pStream << prof;
            pStream.close();
        }
    }
    catch (int e)
    {
        spdlog::critical("Failed to analyze trace");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
