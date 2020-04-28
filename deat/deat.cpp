#include "AtlasUtil/Traces.h"
#include <fstream>
#include <llvm/Support/CommandLine.h>
#include <nlohmann/json.hpp>
#include <queue>
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

map<string, set<int64_t>> kernelMap; //dictionary mapping a name to a set of blocks
string ck;                           ///string denoting the currently processing kernel
map<int64_t, string> UIDMap;         //map of a UID to a kernel name
int64_t uidCounter = 0;              //current uid counter. DO NOT TOUCH
map<uint64_t, int64_t> writeMap;     //dictionary mapping an address to the last uid to write to it
map<int64_t, set<int64_t>> readMap;  //dictionary mapping a uid to the uids it read frome

struct KernelStruct
{
    set<int64_t> blocks;
    string name;
    KernelStruct(set<int64_t> b, string n)
    {
        blocks = move(b);
        name = move(n);
    }
};
struct UIDStruct
{
    string name;
    int64_t UID;
    UIDStruct(string n)
    {
        UID = uidCounter++;
        name = move(n);
        UIDMap[UID] = name;
    }
};

queue<UIDStruct> kernelQueue;
vector<KernelStruct> kernelSet;

bool sizeSort(const KernelStruct &a, const KernelStruct &b)
{
    return a.blocks.size() < b.blocks.size();
}

string getKernel(int64_t block)
{
    for (auto k : kernelSet)
    {
        if (k.blocks.find(block) != k.blocks.end())
        {
            return k.name;
        }
    }
    return "";
}

int64_t block;
void Process(string &key, string &value)
{
    if (key == "BBEnter")
    {
        block = stol(value, nullptr, 0);
        auto k = getKernel(block);
        while (k != ck)
        {
            //we have changed kernel contexts
            if (kernelMap[k].find(block) == kernelMap[k].end())
            {
                //we have left the kernel
                kernelQueue.pop();
                k = kernelQueue.back().name;
            }
            else
            {
                //we have entered a nested kernel
                ck = k;
                kernelQueue.push(UIDStruct(ck));
            }
        }
    }
    else if (key == "LoadAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        readMap[kernelQueue.back().UID].insert(writeMap[address]);
    }
    else if (key == "StoreAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        writeMap[address] = kernelQueue.back().UID;
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
        kernelMap[index] = kernel.get<set<int64_t>>();
        kernelSet.emplace_back(KernelStruct(kernelMap[index], index));
    }
    kernelSet.emplace_back(KernelStruct(j["ValidBlocks"].get<set<int64_t>>(), "-1"));
    std::sort(kernelSet.begin(), kernelSet.end(), sizeSort);

    ck = "-1";
    kernelQueue.push(UIDStruct(ck));
    ProcessTrace(TraceFilename, Process, "Generating DAG", noBar);

    return 0;
}