#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Traces.h"
#include "WorkingSet.h"
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
#include <vector>

using namespace std;
using namespace llvm;

llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));

bool sortKeyInput(string &xin, string &yin)
{
    return WorkingSet::virAddr[xin][2] < WorkingSet::virAddr[yin][2];
}

bool sortKeyInternel(string &xin, string &yin)
{
    return WorkingSet::virAddr[xin][1] < WorkingSet::virAddr[yin][1];
}

int main(int argc, char **argv)
{
    // writeAll is a flag to indicate if working set size for every moment should be monitored
    // or we only need to know the maximum working set size.
    bool writeAll = false;
    cl::ParseCommandLineOptions(argc, argv);
    ProcessTrace(inputTrace, &WorkingSet::Process, "working set analysis", false);

    int64_t internalSampleTime = 50000;
    if (internalSampleTime > WorkingSet::timing)
    {
        internalSampleTime = WorkingSet::timing;
    }
    vector<string> InputkeyVector;    // InputkeyVector is a vector which stores keys of input virtual address map
    vector<string> InternelkeyVector; // InternelkeyVector is a vector which stores keys of internal virtual address map
    // InternelkeyVector is a vector which stores keys of a part of internal virtual address map
    vector<string> InternelSampleKeyVector;
    map<string, vector<int64_t>> InputVirAddr;
    map<string, vector<int64_t>> InternelVirAddr;
    map<string, vector<int64_t>> InternelSampleVirAddr;

    // devide the virtual address map into two, one is internal map whose start time is bigger than 1
    // another is input map whose start time is -1
    //for (map<string, vector<int64_t>>::iterator it = WorkingSet::virAddr.begin(); it != WorkingSet::virAddr.end(); ++it)
    for (auto &it : WorkingSet::virAddr)
    {
        if ((it.second)[1] > 0)
        {
            InternelkeyVector.push_back(it.first);
            InternelVirAddr.insert(pair<string, vector<int64_t>>(it.first, it.second));
            if ((it.second)[1] < internalSampleTime)
            {
                InternelSampleKeyVector.push_back(it.first);
                InternelSampleVirAddr.insert(pair<string, vector<int64_t>>(it.first, it.second));
            }
        }
        else
        {
            InputkeyVector.push_back(it.first);
            InputVirAddr.insert(pair<string, vector<int64_t>>(it.first, it.second));
        }
    }
    if (writeAll)
    {
        // for the internal map, sort the key vector according to the start time
        std::sort(InternelkeyVector.begin(), InternelkeyVector.end(), sortKeyInternel);
        // for the input map, sort the key vector according to the first load time
        std::sort(InputkeyVector.begin(), InputkeyVector.end(), sortKeyInput);
    }
    else
    {
        std::sort(InternelSampleKeyVector.begin(), InternelSampleKeyVector.end(), sortKeyInternel);
    }

    uint64_t alivenumber = WorkingSet::inputSize;
    vector<uint64_t> outputWS;
    vector<uint64_t> inputWS;
    vector<uint64_t> internalWS;
    uint64_t maxInput = 0;
    uint64_t maxOutput = 0;
    uint64_t maxinternal = 0;
    vector<string> outputList;

    if (writeAll)
    {
        for (int64_t time = 0; time < WorkingSet::timing; time++)
        {
            vector<string> inputList;
            vector<string> timeline;
            //for (vector<string>::iterator itv = InternelkeyVector.begin(); itv != InternelkeyVector.end(); ++itv)
            for (auto itv = InternelkeyVector.begin(); itv != InternelkeyVector.end();)
            {
                //string key = *itv;
                if (!InternelVirAddr[*itv].empty())
                {

                    if (InternelVirAddr[*itv][1] > time)
                    {
                        break;
                    }
                    if (InternelVirAddr[*itv].size() > 2)
                    {
                        if (InternelVirAddr[*itv][1] <= time && InternelVirAddr[*itv][InternelVirAddr[*itv].size() - 1] > time)
                        {
                            timeline.push_back(*itv);
                        }
                        else if (time > InternelVirAddr[*itv][InternelVirAddr[*itv].size() - 1])
                        {
                            InternelVirAddr.erase(*itv);
                            InternelkeyVector.erase(itv);
                        }
                    }
                    else if (std::find(outputList.begin(), outputList.end(), *itv) == outputList.end())
                    {

                        outputList.push_back(*itv);
                        InternelVirAddr.erase(*itv);
                        InternelkeyVector.erase(itv);
                    }
                }
            }
            if (maxOutput < outputList.size())
            {
                maxOutput = outputList.size();
            }
            if (maxinternal < timeline.size())
            {
                maxinternal = timeline.size();
            }
            if (writeAll)
            {
                outputWS.push_back(outputList.size());
                internalWS.push_back(timeline.size());
            }
            timeline.clear();
            for (auto itv = InputkeyVector.begin(); itv != InputkeyVector.end(); ++itv)
            {
                string key = *itv;
                if (InputkeyVector.size() == 1)
                {
                    inputList.push_back(key);
                    break;
                }

                if (InputVirAddr.count(key) > 0)
                {
                    if (InputVirAddr[key].size() > 2)
                    {
                        if (InputVirAddr[key][2] > time)
                        {
                            break;
                        }
                    }

                    if (time > InputVirAddr[key][InputVirAddr[key].size() - 1])
                    {
                        inputList.push_back(key);
                        InputVirAddr.erase(key);
                        InputkeyVector.erase(itv);
                    }
                }
            }
            alivenumber = alivenumber - inputList.size();
            if (maxInput < alivenumber)
            {
                maxInput = alivenumber;
            }
            if (writeAll)
            {
                inputWS.push_back(alivenumber);
            }
            if (time % 1000 == 0)
            {
                float ProcessRatio = (float)time / WorkingSet::timing;
                printf("process time :%ld, %ld, %.5f \n", time, WorkingSet::timing, ProcessRatio);
            }
        }
    }
    else
    {
        for (auto &key : InternelkeyVector)
        {
            if (InternelVirAddr[key].size() < 3)
            {
                maxOutput++;
            }
        }
        for (int64_t time = 0; time < internalSampleTime; time++)
        {
            vector<string> timeline;
            for (auto itv = InternelSampleKeyVector.begin(); itv != InternelSampleKeyVector.end()-1;)
            {
                string key = *itv;

                if (!InternelSampleVirAddr[key].empty())
                {

                    if (InternelSampleVirAddr[key][1] > time)
                    {
                        itv++;
                        break;
                    }
                    if (InternelSampleVirAddr[key].size() > 2)
                    {
                        if (InternelSampleVirAddr[key][1] <= time && InternelSampleVirAddr[key][InternelSampleVirAddr[key].size() - 1] > time)
                        {
                            timeline.push_back(key);
                            itv++;
                        }
                        else if (time > InternelSampleVirAddr[key][InternelSampleVirAddr[key].size() - 1])
                        {
                            InternelSampleVirAddr.erase(key);
                            itv = InternelSampleKeyVector.erase(itv);
                        }
                        else
                        {
                            itv++;
                        }
                    }
                    else
                    {                        
                        InternelSampleVirAddr.erase(key);
                        itv = InternelSampleKeyVector.erase(itv);
                    }
                }
                else
                {
                     ++itv;
                }
            }
            if (maxinternal < timeline.size())
            {
                maxinternal = timeline.size();
            }
        }
    }
    maxInput = alivenumber;
    printf("maxInput: %lu \n maxinternal: %lu \n maxOutput: %lu \n", maxInput, maxinternal, maxOutput);

    if (writeAll)
    {
        ofstream f;
        f.open("./inputWorkingSet.txt");
        for (uint64_t j : inputWS)
        {
            f << inputWS[j] << endl;
        }
        f.close();
        f.open("./outputWorkingSet.txt");
        for (uint64_t j : outputWS)
        {
            f << outputWS[j] << endl;
        }
        f.close();
        f.open("./internalWorkingSet.txt");
        for (uint64_t j : internalWS)
        {
            f << internalWS[j] << endl;
        }
        f.close();
    }
}