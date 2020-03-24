#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Traces.h"
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

llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
// cl::opt<int> LogLevel("v", cl::desc("Logging level"), cl::value_desc("logging level"), cl::init(4));
// cl::opt<string> LogFile("l", cl::desc("Specify log filename"), cl::value_desc("log file"));
int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    
    ProcessTrace(inputTrace, &WorkingSet::Process, "working set analysis", false);
}