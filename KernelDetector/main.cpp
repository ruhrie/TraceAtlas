
#include <vector>
#include <set>
#include "EncodeDetect.h"
#include "EncodeExtract.h"
#include <string>
#include <nlohmann/json.hpp>
#include <llvm/Support/CommandLine.h>

using namespace std;
using namespace llvm;

llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
llvm::cl::opt<float> threshold("t", cl::desc("The threshold of block grouping required to complete a kernel."));
llvm::cl::opt<int> hotThreshold("ht", cl::desc("The threshold of block grouping required to complete a kernel."));
llvm::cl::opt<string> kernelFile("k", llvm::cl::desc("Specify output json name"), llvm::cl::value_desc("output filename"));
llvm::cl::opt<string> csvFile("c", llvm::cl::desc("The intermediate csv file containing type 1 kernels. Mandatory of initial state > 0."), llvm::cl::value_desc("output filename"));
llvm::cl::opt<int> initStage("is", llvm::cl::desc("The initial stage of the tool. Defaults to 0."), llvm::cl::value_desc("output filename"));
llvm::cl::opt<int> finalStage("fs", llvm::cl::desc("The final stage of the tool. Defaults to 2."), llvm::cl::value_desc("output filename"));
llvm::cl::opt<string> OutputDAG("d", llvm::cl::desc("Specify the DAG file name."), llvm::cl::value_desc("output filename"));

int main()
{
    char sourceFile[] = "./testing/bubbleSort_0_0.trc";
    if( initStage == 0)
    {
        std::vector< std::set< int > > type1Kernels = DetectKernels(inputTrace, threshold, hotThreshold, false);
        std::cout << "Detected " << type1Kernels.size() << " type 1 kernels." << std::endl;
    }
    else
    {
        // have to interpret csvFile and assign it to type1Kernels
    }
    std::map< int, std::vector< int > > type2Kernels = ExtractKernels(inputTrace, type1Kernels, false);
    std::cout << "Detected " << type2Kernels.size() << " type 2 kernels." << std::endl;

    nlohmann::json outputJson;
    for( auto key : type2Kernels )
    {
        outputJson[key] = key.second;
    }

    ofstream oStream(kernelFile);
    oStream << outputJson;
    oStream.close();
    /*
    for( auto entry : type1Kernels )
    {
        for( auto index : entry )
        {
            std::cout << index << " , ";
        }
        std::cout << "\n\n";
    }
    for( auto entry : type2Kernels )
    {
        for( auto index : entry.second )
        {
            std::cout << index << " , ";
        }
        std::cout << "\n\n";
    }
    */
    return 0;
}
