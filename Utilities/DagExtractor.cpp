#include <zlib.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include "json.hpp"
#include <llvm/Support/CommandLine.h>
using namespace llvm;
using namespace std;

#define BLOCK_SIZE 4096

cl::opt<std::string> InputFilename("t", cl::desc("Specify input trace"), cl::value_desc("trace filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"));
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);

static int UID = 0;

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);

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
    for (auto &[key, value] : j.items())
    {
        set<int> kernelSet = value;
        kernelMap[key] = kernelSet;
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
    int size = inputTrace.tellg();
    inputTrace.seekg(0, ios_base::beg);

    string result = "";
    bool notDone = true;
    std::vector<int> basicBlocks;
    int lastBlock;
    string currentKernel = "-1";
    int status = 0;
    int currentUid = -1;
    while (notDone)
    {
        status++;
        inputTrace.readsome(dataArray, BLOCK_SIZE);
        strm.next_in = (Bytef *)dataArray;
        strm.avail_in = inputTrace.gcount();
        while (strm.avail_in != 0)
        {
            strm.next_out = (Bytef *)decompressedArray;
            strm.avail_out = BLOCK_SIZE;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);
            //we have now decompressed the data
            unsigned int have = BLOCK_SIZE - strm.avail_out;
            for (int i = 0; i < have; i++)
            {
                result += decompressedArray[i];
            }
            std::stringstream stringStream(result);
            std::vector<std::string> split;
            string segment;
            while (getline(stringStream, segment, '\n'))
            {
                split.push_back(segment);
            }
            int l = 0;
            for (string line : split)
            {
                l++;
                if (line == split.back() && result.back() != '\n')
                {
                    result = line;
                }
                else
                {
                    std::stringstream lineStream(line);
                    string key, value;
                    getline(lineStream, key, ':');
                    getline(lineStream, value, ':');
                    if (key == "BasicBlock")
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
                }
            }
            if (result.back() == '\n')
            {
                result = "";
            }
        }

        notDone = ret != Z_STREAM_END;
        if (status % 100 == 0)
        {
            std::cout << "Currently reading block " << to_string(status) << " of " << to_string(size / BLOCK_SIZE) << "\n";
        }
    }
    //trace is now fully read
    inflateEnd(&strm);
    inputTrace.close();

    nlohmann::json jOut;
    jOut["KernelInstanceMap"] = kernelIdMap;
    jOut["ConsumerMap"] = consumerMap;

    if (OutputFilename != "")
    {
        std::ofstream file;
        file.open(OutputFilename);
        file << jOut;
        file.close();
    }
    else
    {
        cout << jOut << "\n";
    }

    return 0;
}
