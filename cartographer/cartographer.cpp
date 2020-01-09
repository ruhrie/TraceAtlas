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
#include <string>
#include <tuple>
#include <vector>

using namespace std;
using namespace llvm;

llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
llvm::cl::opt<float> threshold("t", cl::desc("The threshold of block grouping required to complete a kernel."), llvm::cl::value_desc("float"), llvm::cl::init(0.95));
llvm::cl::opt<int> hotThreshold("ht", cl::desc("The minimum instance count for a basic block to be a seed."), llvm::cl::init(512));
llvm::cl::opt<string> kernelFile("k", llvm::cl::desc("Specify output json name"), llvm::cl::value_desc("kernel filename"), llvm::cl::init("kernel.json"));
llvm::cl::opt<string> profileFile("p", llvm::cl::desc("Specify profile json name"), llvm::cl::value_desc("profile filename"));
llvm::cl::opt<string> bitcodeFile("b", llvm::cl::desc("Specify bitcode name"), llvm::cl::value_desc("bitcode filename"), llvm::cl::Required);
int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    std::vector<std::set<int>> type1Kernels;
    type1Kernels = DetectKernels(inputTrace, threshold, hotThreshold, false);
    std::cout << "Detected " << type1Kernels.size() << " type 1 kernels." << std::endl;
    std::map<int, std::set<int>> type2Kernels = ExtractKernels(inputTrace, type1Kernels, false);
    std::cout << "Detected " << type2Kernels.size() << " type 2 kernels." << std::endl;
    map<int, set<int>> type3Kernels = SmoothKernel(type2Kernels, bitcodeFile);

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
        assert(!bitcodeFile.empty());
        LLVMContext context;
        SMDiagnostic smerror;
        unique_ptr<Module> sourceBitcode;
        try
        {
            sourceBitcode = parseIRFile(bitcodeFile, smerror, context);
        }
        catch (exception e)
        {
            std::cerr << "Couldn't open input bitcode file: " << bitcodeFile << "\n";
            return EXIT_FAILURE;
        }
        nlohmann::json prof = ProfileKernels(type3Kernels, sourceBitcode.get());
        ofstream pStream(profileFile);
        pStream << prof;
        pStream.close();
    }
    return 0;
}
