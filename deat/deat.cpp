#include "AtlasUtil/Traces.h"
#include <fstream>
#include <llvm/Support/CommandLine.h>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

using namespace llvm;
using namespace std;

cl::opt<int> LogLevel("v", cl::desc("Logging level"), cl::value_desc("logging level"), cl::init(4));
cl::opt<string> LogFile("l", cl::desc("Specify log filename"), cl::value_desc("log file"));
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);
cl::opt<std::string> TraceFilename(cl::Positional, cl::value_desc("Trace filename"), cl::Required);
llvm::cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));

map<string, set<string>> parentMap;

map<string, set<int>> kernelMap;

vector<string> kernels;

map<string, string> currentKernel;
map<string, int64_t> lastblock;
map<string, int64_t> currentUid;
map<string, map<int, string>> kernelIdMap;
map<string, int64_t> UID;
map<string, map<uint64_t, int64_t>> writeMap;

map<string, map<int, set<int>>> consumerMap;

int64_t block;
void Process(vector<string> &values)
{
    string key = values[0];
    string value = values[1];
    if (key == "BBEnter")
    {
        block = stol(value, nullptr, 0);
        for (const auto &kernel : kernels)
        {
            if (kernelMap[kernel].find(block) != kernelMap[kernel].end())
            {
                //this block is in the kernel we care about
                lastblock[kernel] = block;
                //first we ask are we supposed be in a sub kernel
                //or should we be changing kernel contexts
                if (currentKernel[kernel] == "-1" || kernelMap[currentKernel[kernel]].find(block) == kernelMap[currentKernel[kernel]].end())
                {
                    //if so we will set our new context
                    string innerKernel = "-1";
                    for (auto &[k, s] : kernelMap)
                    {
                        if (s.find(block) != s.end())
                        {
                            //we found a kernel that contains the block, but we now need to check that it is a child
                            if (parentMap[k].find(kernel) != parentMap[k].end())
                            {
                                //valid child
                                innerKernel = k;
                                break;
                            }
                        }
                    }
                    currentKernel[kernel] = innerKernel;
                    if (innerKernel != "-1")
                    {
                        currentUid[kernel] = UID[kernel];
                        kernelIdMap[kernel][UID[kernel]++] = currentKernel[kernel];
                    }
                }
            }
            else
            {
                //it isn't and we should post results
                //cout << "hi";
            }
        }
    }
    else if (key == "LoadAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        for (const auto &kernel : kernels)
        {
            if (kernelMap[kernel].find(block) != kernelMap[kernel].end())
            {
                int prodUid = writeMap[kernel][address];
                if (prodUid != -1 && prodUid != currentUid[kernel])
                {
                    consumerMap[kernel][currentUid[kernel]].insert(prodUid);
                }
                else
                {
                }
            }
        }
    }
    else if (key == "StoreAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        for (const auto &kernel : kernels)
        {
            if (kernelMap[kernel].find(block) != kernelMap[kernel].end())
            {
                writeMap[kernel][address] = currentUid[kernel];
            }
            else
            {
            }
        }
    }
}

int main(int argc, char *argv[])
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

    ifstream inputJson(KernelFilename);
    nlohmann::json j;
    inputJson >> j;
    inputJson.close();

    for (auto &[k, l] : j["Kernels"].items())
    {
        string index = k;
        nlohmann::json kernel = l["Blocks"];
        kernelMap[index] = kernel.get<set<int>>();
        kernels.push_back(k);
        currentUid[k] = -1;
        currentKernel[k] = "-1";
    }
    for (auto &b : j["ValidBlocks"])
    {
        kernelMap["-2"].insert(b.get<int64_t>());
    }
    kernels.emplace_back("-2");

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

    ProcessTrace(TraceFilename, Process, "Generating DAG", noBar);

    return 0;
}