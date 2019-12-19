#include "EncodeDetect.h"
#include "EncodeExtract.h"
#include <llvm/Support/CommandLine.h>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <iostream>
#include <vector>

using namespace std;
using namespace llvm;

llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
llvm::cl::opt<float> threshold("t", cl::desc("The threshold of block grouping required to complete a kernel."), llvm::cl::value_desc("float"), llvm::cl::init(0.95));
llvm::cl::opt<int> hotThreshold("ht", cl::desc("The minimum instance count for a basic block to be a seed."), llvm::cl::init(512));
llvm::cl::opt<string> kernelFile("k", llvm::cl::desc("Specify output json name"), llvm::cl::value_desc("kernel filename"), llvm::cl::init("kernel.json"));

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    char sourceFile[] = "./testing/bubbleSort_0_0.trc";
    std::vector<std::set<int>> type1Kernels;
    type1Kernels = DetectKernels(inputTrace, threshold, hotThreshold, false);
    std::cout << "Detected " << type1Kernels.size() << " type 1 kernels." << std::endl;
    std::map<int, std::vector<int>> type2Kernels = ExtractKernels(inputTrace, type1Kernels, false);
    std::cout << "Detected " << type2Kernels.size() << " type 2 kernels." << std::endl;

    nlohmann::json outputJson;
    for (auto key : type2Kernels)
    {
        outputJson[to_string(key.first)] = key.second;
    }

    ofstream oStream(kernelFile);
    oStream << outputJson;
    oStream.close();
    return 0;
}
