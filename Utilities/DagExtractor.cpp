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
static int UID = 0;

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

    //maps
    map<uint64_t, int> writeMap;
    map<int, string> kernelIdMap;
    map<string, set<int>> kernelMap;
    map<string, set<string>> parentMap;

    map<int, set<int>> consumerMap;
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
    for (auto kernel : kernelMap)
    {
        for (auto kernel2 : kernelMap)
        {
            if (kernel.first == kernel2.first)
            {
                continue;
            }
            vector<int> intSet;
            set_difference(kernel.second.begin(), kernel.second.end(), kernel2.second.begin(), kernel2.second.end(), std::inserter(intSet, intSet.begin()));
            if (intSet.size() == 0)
            {
                parentMap[kernel.first].insert(kernel2.first);
            }
        }
    }

    std::set<string> topKernels;
    for (auto kernel : kernelMap)
    {
        if (parentMap.find(kernel.first) == parentMap.end())
        {
            topKernels.insert(kernel.first);
        }
    }

    //now read the trace
    ifstream inputTrace;
    char dataArray[BLOCK_SIZE];
    char decompressedArray[BLOCK_SIZE];

    z_stream strm;
    int ret;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = inflateInit(&strm);
    assert(ret == Z_OK);
    inputTrace.open(InputFilename);

    inputTrace.seekg(0, ios_base::end);
    uint64_t size = inputTrace.tellg();
    inputTrace.seekg(0, ios_base::beg);

    string result = "";
    bool notDone = true;
    std::vector<int> basicBlocks;
    int lastBlock;
    string currentKernel = "-1";
    uint64_t status = 0;
    int currentUid = -1;
    std::string priorLine = "";
    bool seenFirst;
    string segment;

    indicators::ProgressBar bar;
    int previousCount = 0;
    if (!noBar)
    {
        bar.set_prefix_text("Extracting DAG");
        bar.show_elapsed_time();
        bar.show_remaining_time();
        bar.set_bar_width(50);
    }
    int index = 0;

    while (notDone)
    {
        // read a block size of the trace
        inputTrace.readsome(dataArray, BLOCK_SIZE);
        strm.next_in = (Bytef *)dataArray;   // input data to z_lib for decompression
        strm.avail_in = inputTrace.gcount(); // remaining characters in the compressed inputTrace
        while (strm.avail_in != 0)
        {
            // decompress our data
            strm.next_out = (Bytef *)decompressedArray; // pointer where uncompressed data is written to
            strm.avail_out = BLOCK_SIZE - 1;            // remaining space in decompressedArray
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);

            // put decompressed data into a string for splitting
            unsigned int have = BLOCK_SIZE - strm.avail_out;
            decompressedArray[have - 1] = '\0';
            string bufferString(decompressedArray);
            std::stringstream stringStream(bufferString);
            std::string segment;
            std::getline(stringStream, segment, '\n');
            char back = bufferString.back();
            seenFirst = false;

            while (true)
            {
                if (!seenFirst)
                {
                    segment = priorLine + segment;
                    seenFirst = true;
                }
                // split it by the colon between the instruction and value
                std::stringstream itstream(segment);
                std::string key;
                std::string value;
                std::string error;
                std::getline(itstream, key, ':');
                std::getline(itstream, value, ':');
                bool fin = false;
                if (!std::getline(stringStream, segment, '\n'))
                {
                    //cout << "broke" << segment << "\n";
                    if (back == '\n')
                    {
                        fin = true;
                    }
                    else
                    {
                        break;
                    }
                }
                if (key == "BBEnter")
                {
                    int block = stoi(value, 0, 0);
                    lastBlock = block;
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
                    basicBlocks.push_back(block);
                }
                else if (key == "LoadAddress")
                {
                    uint64_t address = stoul(value, 0, 0);
                    int prodUid = writeMap[address];
                    if (prodUid != -1 && prodUid != currentUid)
                    {
                        consumerMap[currentUid].insert(prodUid);
                    }
                }
                else if (key == "StoreAddress")
                {
                    uint64_t address = stoul(value, 0, 0);
                    writeMap[address] = currentUid;
                }
                if (fin)
                {
                    break;
                }
            }
        }

        index++;
        notDone = (ret != Z_STREAM_END);
        if (status > (size / BLOCK_SIZE))
        {
            notDone = false;
        }
        int blocks = size / BLOCK_SIZE;
        float percent = (float)index / (float)blocks * 100.0f;
        if (!noBar)
        {
            bar.set_progress(percent);
            bar.set_postfix_text("Analyzing block " + to_string(index) + "/" + to_string(blocks));
        }
        else
        {
            int iPercent = (int)percent;
            if (iPercent > previousCount + 5)
            {
                previousCount = ((iPercent / 5) + 1) * 5;
                spdlog::info("Completed block {0:d} of {1:d}", index, blocks);
            }
        }
    }
    //trace is now fully read
    inflateEnd(&strm);
    inputTrace.close();

    nlohmann::json jOut;
    jOut["KernelInstanceMap"] = kernelIdMap;
    jOut["ConsumerMap"] = consumerMap;

    std::ofstream file;
    file.open(OutputFilename);
    file << jOut;
    file.close();

    return 0;
}
