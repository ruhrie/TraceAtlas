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
    bool writeAll = false;
    cl::ParseCommandLineOptions(argc, argv);
    ProcessTrace(inputTrace, &WorkingSet::Process, "working set analysis", false);

    //Internal sample: only run at a time range of 50000-100000 but not the total time
    //Because internal working set seems to be always cyclic,
    //we can sample this to predict the maximum internal working set
    int64_t internalSampleTime = 30000;
    if (internalSampleTime > WorkingSet::timing)
    {
        internalSampleTime = WorkingSet::timing;
    }

    auto alivenumber = uint64_t(WorkingSet::inputMapSize);
    // vector outputWS, inputWS, internalWS are used to store work set sizes at each time
    vector<uint64_t> outputWS; 
    vector<uint64_t> inputWS;
    vector<uint64_t> internalWS;

    // the key vectors are used to do fast searching operation to the map
     vector<int64_t> inputKeyVector;
    vector<int64_t> internalKeyVector;

    //used to store max size of input output internal ws
    uint64_t maxInput = 0;
    uint64_t maxOutput = 0;
    uint64_t maxinternal = 0;
   

    //use a vector to store the keys of the virtual address maps, because we need to
    //dynamically erase elements from the map and break the loop in some situation to speed up
    for (int64_t i = 0; i < internalMapSize; i++)
    {
        internalKeyVector.push_back(i);
    }
    for (int64_t i = 0; i < inputMapSize; i++)
    {
        inputKeyVector.push_back(i);
    }

    // a dynamic vector to store output address at each time
     vector<int64_t> outputList;
    // writeAll is a flag to indicate if working set size for every moment should be monitored
    // or we only need to know the maximum working set size.
    if (writeAll)
    {
        //time iteration
        for (int64_t time = 0; time < WorkingSet::timing; time++)
        {
            vector<int64_t> inputList;
            vector<int64_t> timeline;
            //internal virtual address iteration for each time
            for (auto it = internalKeyVector.begin(); it != internalKeyVector.end();)
            {
                if (!internalVirAddr[*it].empty())
                {
                    //the birth time of addresses in map is late than time now, break the loop
                    if (internalVirAddr[*it][1] > time)
                    {
                        break;
                    }
                    //if the size of the address in the map is bigger than 2, it's not an output address
                    if (internalVirAddr[*it].size() > 2)
                    {
                        //  begin < t < end, it is a living internal address
                        if (internalVirAddr[*it][1] <= time && internalVirAddr[*it][internalVirAddr[*it].size() - 1] > time)
                        {
                            timeline.push_back(*it);
                            it++;
                        }
                        //  t > end, it is a ended internal address, erasing this from the map to speed up
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
                    // if the address is not in the output list, then push it into the list and erase it from the map to speed up
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
            // count the maximum size for output and internal working set
            if (maxOutput < outputList.size())
            {
                maxOutput = outputList.size();
            }
            if (maxinternal < timeline.size())
            {
                maxinternal = timeline.size();
            }
            // save the value of the size of output and internal working set at each time
            outputWS.push_back(outputList.size());
            internalWS.push_back(timeline.size());
            timeline.clear();
            //input virtual address iteration for each time
            for (auto it = inputKeyVector.begin(); it != inputKeyVector.end();)
            {
                if (!inputVirAddr[*it].empty())
                {
                    //the first loading time of addresses in input map is late than time now, break the loop
                    if (inputVirAddr[*it][2] > time)
                    {
                        break;
                    }
                    //  t > end, it is a ended input address, erasing this from the map to speed up
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
    // not storing the working set size at each time, only counting the maximum size
    // using sample internal time not total time for time iteration
    else
    {
        for (auto &key : internalKeyVector)
        {
            //if the size of the address in the map is smaller than 3, it's an output address
            if (internalVirAddr[key].size() < 3)
            {
                maxOutput++;
            }
        }
        //time iteration
        for (int64_t time = 0; time < internalSampleTime; time++)
        {
            vector<int64_t> inputList;
            vector<int64_t> timeline;
            //internal virtual address iteration for each time
            for (auto it = internalKeyVector.begin(); it != internalKeyVector.end();)
            {
                if (!internalVirAddr[*it].empty())
                {
                    //the birth time of addresses in map is late than time now, break the loop
                    if (internalVirAddr[*it][1] > time)
                    {
                        break;
                    }
                    //if the size of the address in the map is bigger than 2, it's not an output address
                    if (internalVirAddr[*it].size() > 2)
                    {
                        //  begin < t < end, it is a living internal address
                        if (internalVirAddr[*it][1] <= time && internalVirAddr[*it][internalVirAddr[*it].size() - 1] > time)
                        {
                            timeline.push_back(*it);
                            it++;
                        }
                        //  t > end, it is a ended internal address, erasing this from the map to speed up
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
                    // if the address is not in the output list, then push it into the list and erase it from the map to speed up
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