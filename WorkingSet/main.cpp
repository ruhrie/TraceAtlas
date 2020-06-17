#include "AtlasUtil/Traces.h"
#include "inc/WorkingSet.h"
#include <llvm/Support/CommandLine.h>
#include <ostream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

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

    // Read input kernel file
    ifstream inputJson;
    nlohmann::json j;
    try
    {
        inputJson.open(kernelFile);
        inputJson >> j;
        inputJson.close();
    }
    catch (exception &e)
    {
        std::cerr << "Couldn't open input json file: " << kernelFile << "\n";
        std::cerr << e.what() << '\n';
        spdlog::critical("Failed to open kernel file: " + kernelFile);
        return EXIT_FAILURE;
    }

    // initialize and parse trace
    try
    {
        WorkingSet::Setup(j);
        spdlog::info("Started WorkingSet analysis.");
        ProcessTrace(inputTrace, &WorkingSet::Process, "Parsing Load and Store sets.", noBar);
        WorkingSet::InternalSet();
        //WorkingSet::PrintOutput();
        WorkingSet::PrintSizes();
    }
    catch (int e)
    {
        spdlog::critical("Failed to analyze trace");
        return EXIT_FAILURE;
    }

    return 0;
}