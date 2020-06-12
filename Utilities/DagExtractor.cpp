#include "AtlasUtil/Traces.h"
#include <algorithm>
#include <fstream>
#include <indicators/progress_bar.hpp>
#include <llvm/Support/CommandLine.h>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <vector>
#include <zlib.h>
using namespace llvm;
using namespace std;

#define BLOCK_SIZE 4096

cl::opt<std::string> InputFilename("t", cl::desc("Specify input trace"), cl::value_desc("trace filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);
llvm::cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));
cl::opt<int> LogLevel("v", cl::desc("Logging level"), cl::value_desc("logging level"), cl::init(4));
cl::opt<string> LogFile("l", cl::desc("Specify log filename"), cl::value_desc("log file"));
cl::opt<string> DotFile("d", cl::desc("Specify dot filename"), cl::value_desc("dot file"));
static int UID = 0;

string currentKernel = "-1";
int currentUid = -1;

//maps
map<uint64_t, int> writeMap;
map<int, string> kernelIdMap;
map<string, set<int>> kernelMap;
map<string, set<string>> parentMap;
map<int, set<int>> consumerMap;

string GenerateGraph(map<int, string> instanceMap, const map<int, set<int>> &consumerMap)
{
    string result = "digraph{\n";

    for (int i = 0; i < instanceMap.size(); i++)
    {
        result += "\t" + to_string(i) + " [label=" + instanceMap[i] + "]\n";
    }

    if (instanceMap.size() > 1)
    {
        for (int i = 1; i < instanceMap.size(); i++)
        {
            result += "\t" + to_string(i - 1) + " -> " + to_string(i) + ";\n";
        }
    }

    for (const auto &cons : consumerMap)
    {
        for (auto c : cons.second)
        {
            result += "\t" + instanceMap[cons.first] + " -> " + to_string(c) + " [style=dashed];\n";
        }
    }

    result += "}";
    return result;
}

void Process(string &key, string &value)
{
    if (key == "BBEnter")
    {
        int block = stoi(value, nullptr, 0);
        if (currentKernel == "-1" || kernelMap[currentKernel].find(block) == kernelMap[currentKernel].end())
        {
            //we aren't in the same kernel as last time
            string innerKernel = "-1";
            for (auto k : kernelMap)
            {
                if (k.second.find(block) != k.second.end())
                {
                    //we have a matching kernel
                    innerKernel = k.first;
                    break;
                }
            }
            currentKernel = innerKernel;
            if (innerKernel != "-1")
            {
                currentUid = UID;
                kernelIdMap[UID++] = currentKernel;
            }
        }
    }
    else if (key == "LoadAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        int prodUid = writeMap[address];
        if (prodUid != -1 && prodUid != currentUid)
        {
            consumerMap[currentUid].insert(prodUid);
        }
    }
    else if (key == "StoreAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        writeMap[address] = currentUid;
    }
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);

    if (!LogFile.empty())
    {
        auto file_logger = spdlog::basic_logger_mt("dag_logger", LogFile);
        spdlog::set_default_logger(file_logger);
    }

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

    //read the json
    ifstream inputJson(KernelFilename);
    nlohmann::json j;
    inputJson >> j;
    inputJson.close();

    for (auto &[k, l] : j["Kernels"].items())
    {
        string index = k;
        nlohmann::json kernel = l["Blocks"];
        kernelMap[index] = kernel.get<set<int>>();
    }

    //get parent child relationships
    for (const auto &kernel : kernelMap)
    {
        for (const auto &kernel2 : kernelMap)
        {
            if (kernel.first == kernel2.first)
            {
                continue;
            }
            vector<int> intSet;
            set_difference(kernel.second.begin(), kernel.second.end(), kernel2.second.begin(), kernel2.second.end(), std::inserter(intSet, intSet.begin()));
            if (intSet.empty())
            {
                parentMap[kernel.first].insert(kernel2.first);
            }
        }
    }

    std::set<string> topKernels;
    for (const auto &kernel : kernelMap)
    {
        if (parentMap.find(kernel.first) == parentMap.end())
        {
            topKernels.insert(kernel.first);
        }
    }

    ProcessTrace(InputFilename, Process, "Generating DAG", noBar);

    nlohmann::json jOut;
    jOut["KernelInstanceMap"] = kernelIdMap;
    jOut["ConsumerMap"] = consumerMap;

    std::ofstream file;
    file.open(OutputFilename);
    file << jOut;
    file.close();

    if (!DotFile.empty())
    {
        ofstream dStream(DotFile);
        auto graph = GenerateGraph(kernelIdMap, consumerMap);
        dStream << graph << "\n";
        dStream.close();
    }

    spdlog::info("Successfully extracted DAG");

    return 0;
}
