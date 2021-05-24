#include "AtlasUtil/IO.h"
#include <llvm/Support/CommandLine.h>
#include <map>

using namespace llvm;
using namespace std;

cl::opt<std::string> BlockInfoFilename("bi", cl::value_desc("BlockInfo Json filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::value_desc("Output Json filename"), cl::Required);

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    auto blockLabels = ReadBlockLabels(BlockInfoFilename);

    std::ofstream file;
    nlohmann::json jOut;
    for (const auto &block : blockLabels)
    {
        uint64_t max = 0;
        string maxLabel;
        for (const auto &label : block.second)
        {
            if (label.second > max)
            {
                maxLabel = label.first;
            }
        }
        jOut[to_string(block.first)] = maxLabel;
    }
    file.open(OutputFilename);
    file << jOut;
    file.close();
}