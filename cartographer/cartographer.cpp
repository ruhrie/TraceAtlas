#include "EncodeDetect.h"
#include "EncodeExtract.h"
#include "Smoothing.h"
#include "profile.h"
#include <iostream>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/spdlog.h>
#include <string>
#include <tuple>
#include <vector>

using namespace std;
using namespace llvm;

bool noProgressBar;

llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
llvm::cl::opt<float> threshold("t", cl::desc("The threshold of block grouping required to complete a kernel."), llvm::cl::value_desc("float"), llvm::cl::init(0.95));
llvm::cl::opt<int> hotThreshold("ht", cl::desc("The minimum instance count for a basic block to be a seed."), llvm::cl::init(512));
llvm::cl::opt<string> kernelFile("k", llvm::cl::desc("Specify output json name"), llvm::cl::value_desc("kernel filename"), llvm::cl::init("kernel.json"));
llvm::cl::opt<string> profileFile("p", llvm::cl::desc("Specify profile json name"), llvm::cl::value_desc("profile filename"));
llvm::cl::opt<string> bitcodeFile("b", llvm::cl::desc("Specify bitcode name"), llvm::cl::value_desc("bitcode filename"), llvm::cl::Required);
llvm::cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));
int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    noProgressBar = noBar;
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
    try
    {
        std::vector<std::set<int>> type1Kernels;
        spdlog::info("Started analysis");
        type1Kernels = DetectKernels(inputTrace, threshold, hotThreshold, false);
        spdlog::info("Detected " + to_string(type1Kernels.size()) + " type 1 kernels");
        std::map<int, std::set<int>> type2Kernels = ExtractKernels(inputTrace, type1Kernels, sourceBitcode.get());
        spdlog::info("Detected " + to_string(type2Kernels.size()) + " type 2 kernels");
        map<int, set<int>> type3Kernels = SmoothKernel(type2Kernels, bitcodeFile);
        spdlog::info("Detected " + to_string(type3Kernels.size()) + " type 3 kernels");

        nlohmann::json outputJson;
        for (auto key : type3Kernels)
        {
            outputJson[to_string(key.first)] = key.second;
        }
        ofstream oStream(kernelFile);
        oStream << outputJson;
        oStream.close();
        if (!profileFile.empty())
        {
            nlohmann::json prof = ProfileKernels(type3Kernels, sourceBitcode.get());
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
