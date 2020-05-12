#include "AtlasUtil/Traces.h"
#include <algorithm>
#include <fstream>
#include <future>
#include <indicators/progress_bar.hpp>
#include <llvm/Support/CommandLine.h>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <vector>
#include <zlib.h>
using namespace llvm;
using namespace std;

#define BLOCK_SIZE 4096

llvm::cl::list<string> inputTraces(llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filenames"), cl::Positional);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);
llvm::cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));
cl::opt<int> LogLevel("v", cl::desc("Logging level"), cl::value_desc("logging level"), cl::init(4));
cl::opt<string> LogFile("l", cl::desc("Specify log filename"), cl::value_desc("log file"));
atomic<int> UID = 0;

thread_local string currentKernel = "-1";
thread_local int currentUid = -1;

//maps
map<uint64_t, int> writeMap;
map<int, string> kernelIdMap;
map<string, set<int>> kernelMap;
map<string, set<string>> parentMap;
map<int, set<int>> consumerMap;
map<thread::id, mutex> mutexMap;

struct MemoryStruct
{
    uint64_t time = 0;
    mutex *m;
    MemoryStruct(uint64_t t, mutex *i)
    {
        time = t;
        m = i;
    }

    bool operator<(const MemoryStruct &b) const
    {
        return time > b.time;
    }
};

priority_queue<MemoryStruct> workingQueue;
int currentHold = 0;
mutex masterMutex;
mutex queueMutex;

void Process(vector<string> &values)
{
    string key = values[0];
    string value = values[1];
    auto id = this_thread::get_id();

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
                currentUid = UID++;
                kernelIdMap[currentUid] = currentKernel;
            }
        }
    }
    else if (key == "LoadAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        uint64_t time = stoul(values[2], nullptr, 0);
        auto m = &mutexMap[id];
        queueMutex.lock();
        workingQueue.push(MemoryStruct(time, m));
        queueMutex.unlock();
        m->lock();
        int prodUid = writeMap[address];
        if (prodUid != -1 && prodUid != currentUid)
        {
            consumerMap[currentUid].insert(prodUid);
        }
        masterMutex.unlock();
    }
    else if (key == "StoreAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        uint64_t time = stoul(values[2], nullptr, 0);
        auto m = &mutexMap[id];
        queueMutex.lock();
        workingQueue.push(MemoryStruct(time, m));
        queueMutex.unlock();
        m->lock();
        writeMap[address] = currentUid;
        masterMutex.unlock();
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

    //this is the real logic
    vector<shared_ptr<thread>> threads;
    auto completeThreads = new atomic<int>(0);
    int i = 0;
    for (auto &tr : inputTraces)
    {
        auto t = std::make_shared<thread>(ProcessTrace, tr, Process, "Generating Dag", true, completeThreads);
        auto id = t->get_id();
        mutexMap[id].lock();
        threads.push_back(t);
    }
    sleep(1);
    while (true)
    {
        if (!workingQueue.empty())
        {
            masterMutex.lock();
            queueMutex.lock();
            auto top = workingQueue.top();
            top.m->unlock();
            workingQueue.pop();
            queueMutex.unlock();
        }
        else
        {
            if (*completeThreads != inputTraces.size())
            {
                continue;
            }
            break;
        }
    }
    for (auto &t : threads)
    {
        t->join();
    }

    delete completeThreads;

    nlohmann::json jOut;
    jOut["KernelInstanceMap"] = kernelIdMap;
    jOut["ConsumerMap"] = consumerMap;

    std::ofstream file;
    file.open(OutputFilename);
    file << jOut;
    file.close();

    spdlog::info("Successfully extracted DAG");

    return 0;
}
