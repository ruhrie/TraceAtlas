#include "AtlasUtil/Traces.h"
#include <algorithm>
#include <fstream>
#include <llvm/Support/CommandLine.h>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

using namespace llvm;
using namespace std;

cl::opt<std::string> InputFilename("i", cl::desc("Specify input trace"), cl::value_desc("trace filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);
llvm::cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));

map<int64_t, set<string>> labels;
set<string> currentLabels;
map<string,set<int64_t>> labelsToblock;

void Process(string &key, string &value)
{
    if (key == "BBExit")
    {
        int64_t block = stol(value, nullptr, 0);
        labels[block].insert(currentLabels.begin(), currentLabels.end());

        for (auto i : currentLabels)
        {
            labelsToblock[i].insert(block);
        }
    }
    else if (key == "KernelEnter")
    {
        currentLabels.insert(value);
    }
    else if (key == "KernelExit")
    {
        currentLabels.erase(value);
    }
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    ProcessTrace(InputFilename, Process, "Generating JR", noBar);
    // map<set<string>,int64_t> labelsToblock;

    // for (auto i : labels)
    // {
    //     labelsToblock[i.second] = i.first;
    // }
    std::ofstream file;
    nlohmann::json jOut;
    for (const auto &pair : labels)
    {
        string label = std::to_string(pair.first);
        jOut[label] = pair.second;
    }

    int count = 0;
    for (auto i : labelsToblock)
    {
        jOut["Kernels"][to_string(count)]["Blocks"] = i.second;
        jOut["Kernels"][to_string(count)]["Label"] = i.first;
        count++;
    }


    
    file.open(OutputFilename);
    file << std::setw(4) << jOut << std::endl;
    file.close();
}