#include "AtlasUtil/Format.h"
#include "AtlasUtil/IO.h"
#include "AtlasUtil/Traces.h"
#include "Graph.h"
#include <algorithm>
#include <fstream>
#include <llvm/Support/CommandLine.h>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

using namespace llvm;
using namespace std;

cl::opt<std::string> InputFilename("i", cl::desc("Specify csv file"), cl::value_desc("csv filename"), cl::Required);
cl::opt<std::string> BitcodeFilename("b", cl::desc("Specify bitcode file"), cl::value_desc("bitcode filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);
cl::opt<bool> Preformat("pf", llvm::cl::desc("Bitcode is preformatted"), llvm::cl::value_desc("Bitcode is preformatted"));

map<int64_t, set<string>> labels;
set<string> currentLabels;

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    auto M = LoadBitcode(BitcodeFilename);
    auto csvData = LoadCSV(InputFilename);
    auto mp = M.get();
    if (!Preformat)
    {
        Format(mp);
    }
    BuildGraph(mp, csvData);
    return 0;
}