#include "AtlasUtil/Traces.h"
#include <llvm/Support/CommandLine.h>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <pair>

using namespace std;
using namespace llvm;

llvm::cl::opt<string> kernelFile("k", llvm::cl::desc("Specify output json name"), llvm::cl::value_desc("kernel filename"), llvm::cl::init("kernel.json"));
llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
llvm::cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));
cl::opt<int> LogLevel("v", cl::desc("Logging level"), cl::value_desc("logging level"), cl::init(4));
int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);

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

    try
    {
        spdlog::info("Started analysis");
        ProcessTrace(inputTrace, &TypeOne::Process, "Detecting type 1 kernels", noBar);
        auto type1Kernels = TypeOne::Get();
        spdlog::info("Detected " + to_string(type1Kernels.size()) + " type 1 kernels");

        for (auto &[block, count] : TypeOne::blockCount)
        {
            if (count != 0)
            {
                ValidBlocks.insert(block);
            }
        }

        TypeTwo::Setup(M, type1Kernels);
        ProcessTrace(inputTrace, &TypeTwo::Process, "Detecting type 2 kernels", noBar);
        auto type2Kernels = TypeTwo::Get();
        spdlog::info("Detected " + to_string(type2Kernels.size()) + " type 2 kernels");