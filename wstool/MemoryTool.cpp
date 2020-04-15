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
using namespace WorkingSet;
llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));

int main(int argc, char **argv)
{
    // writeAll is a flag to indicate if working set size for every moment should be monitored
    // or we only need to know the maximum working set size.
    bool writeAll = false;
    cl::ParseCommandLineOptions(argc, argv);
    ProcessTrace(inputTrace, &WorkingSet::Process, "working set analysis", false);

    int64_t internalSampleTime = 100000;
    if (internalSampleTime > WorkingSet::timing)
    {
        internalSampleTime = WorkingSet::timing;
    }

    auto alivenumber = uint64_t(WorkingSet::inputMapSize);
    vector<uint64_t> outputWS;
    vector<uint64_t> inputWS;
    vector<uint64_t> internalWS;
    vector<int64_t> inputKeyVector;
    vector<int64_t> internalKeyVector;
    uint64_t maxInput = 0;
    uint64_t maxOutput = 0;
    uint64_t maxinternal = 0;
    vector<int64_t> outputList;
    for (int64_t i = 0;i <internalMapSize; i++)
    {
        internalKeyVector.push_back(i);
    }
    for (int64_t i = 0;i <inputMapSize; i++)
    {
        inputKeyVector.push_back(i);
    }

    if (writeAll)
    {
        for (int64_t time = 0; time < WorkingSet::timing; time++)
        {
            vector<int64_t> inputList;
            vector<int64_t> timeline;
            for (auto it = internalKeyVector.begin(); it != internalKeyVector.end();)
            {               
                if (!internalVirAddr[*it].empty())
                {

                    if (internalVirAddr[*it][1] > time)
                    {
                        break;
                    }
                    if (internalVirAddr[*it].size() > 2)
                    {
                        if (internalVirAddr[*it][1] <= time && internalVirAddr[*it][internalVirAddr[*it].size() - 1] > time)
                        {
                            timeline.push_back(*it);
                             it++;
                        }
                        else if (time > internalVirAddr[*it][internalVirAddr[*it].size() - 1])
                        {
                            internalVirAddr.erase(*it);
                            it = internalKeyVector.erase(it);
                        }
                        else
                        {
                             it++;
                        }
                    }
                    else if (std::find(outputList.begin(), outputList.end(), *it) == outputList.end())
                    {

                        outputList.push_back(*it);
                        internalVirAddr.erase(*it);
                        it = internalKeyVector.erase(it);
                    }
                    else
                    {
                        it++;
                    }
                }
                else
                {
                    it++;
                }
            }
            if (maxOutput <  outputList.size())
            {
                maxOutput = outputList.size();
            }
            if (maxinternal < timeline.size())
            {
                maxinternal = timeline.size();
            }
            outputWS.push_back(outputList.size());
            internalWS.push_back(timeline.size());
            timeline.clear();
            for (auto it = inputKeyVector.begin(); it != inputKeyVector.end();)
            {
                if (!inputVirAddr[*it].empty())
                {
                    if (inputVirAddr[*it][2] > time)
                    {
                        break;
                    }
                    if (time > inputVirAddr[*it][inputVirAddr[*it].size() - 1])
                    {
                        inputList.push_back(*it);
                        inputVirAddr.erase(*it);
                        it = inputKeyVector.erase(it);
                    }
                    else
                    {
                        it++;
                    }
                }
                else
                {
                    it++;
                }                
            }
            alivenumber = alivenumber - inputList.size();

            if (maxInput < alivenumber)
            {
                maxInput = alivenumber;
            }
            inputWS.push_back(alivenumber);
            if (time % 1000 == 0)
            {
                float ProcessRatio = (float)time / WorkingSet::timing;
                printf("process time :%ld, %ld, %.5f \n", time, WorkingSet::timing, ProcessRatio);
            }
        }
    }
    else
    {
        for (auto &key : internalKeyVector)
        {
            if (internalVirAddr[key].size() < 3)
            {
                maxOutput++;
            }
        }
        for (int64_t time = 0; time < internalSampleTime; time++)
        {
            vector<int64_t> inputList;
            vector<int64_t> timeline;
            for (auto it = internalKeyVector.begin(); it != internalKeyVector.end();)
            {               
                if (!internalVirAddr[*it].empty())
                {

                    if (internalVirAddr[*it][1] > time)
                    {
                        break;
                    }
                    if (internalVirAddr[*it].size() > 2)
                    {
                        if (internalVirAddr[*it][1] <= time && internalVirAddr[*it][internalVirAddr[*it].size() - 1] > time)
                        {
                            timeline.push_back(*it);
                             it++;
                        }
                        else if (time > internalVirAddr[*it][internalVirAddr[*it].size() - 1])
                        {
                            cout << "size VirAddr:" << internalVirAddr.size()<<endl;
                            cout << "size KeyVector:" << internalKeyVector.size()<<endl;
                            internalVirAddr.erase(*it);
                            it = internalKeyVector.erase(it);
                        }
                        else
                        {
                             it++;
                        }
                    }
                    else if (std::find(outputList.begin(), outputList.end(), *it) == outputList.end())
                    {

                        outputList.push_back(*it);
                        internalVirAddr.erase(*it);
                        it = internalKeyVector.erase(it);
                    }
                    else
                    {
                        it++;
                    }
                }
                else
                {
                    it++;
                }
            }
            if (maxinternal < timeline.size())
            {
                maxinternal = timeline.size();
            }
            timeline.clear();
            if (time % 1000 == 0)
            {
                float ProcessRatio = (float)time / WorkingSet::timing;
                printf("process time :%ld, %ld, %.5f \n", time, WorkingSet::timing, ProcessRatio);
            }
        }
        maxInput = alivenumber;
    }
    printf("maxInput: %lu \n maxinternal: %lu \n maxOutput: %lu \n", maxInput, maxinternal, maxOutput);

    if (writeAll)
    {
        ofstream f;
        f.open("./inputWorkingSet.txt");
        for (uint64_t j : inputWS)
        {
            f << j << endl;
        }
        f.close();
        f.open("./outputWorkingSet.txt");
        for (uint64_t j : outputWS)
        {
            f << j << endl;
        }
        f.close();
        f.open("./internalWorkingSet.txt");
        for (uint64_t j : internalWS)
        {
            f << j << endl;
        }
        f.close();
    }
}