#include "AtlasUtil/Traces.h"
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

map<int64_t, set<string>> labels;
set<string> currentLabels;

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
}