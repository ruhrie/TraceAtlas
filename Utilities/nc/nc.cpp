#include "AtlasUtil/Format.h"
#include "AtlasUtil/Graph.h"
#include "AtlasUtil/IO.h"
#include "AtlasUtil/Traces.h"
#include "Dijkstra.h"
#include "GraphTransforms.h"
#include "Kernel.h"
#include <One.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <llvm/Support/CommandLine.h>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

using namespace llvm;
using namespace std;

cl::opt<std::string> InputFilename("i", cl::desc("Specify csv file"), cl::value_desc("csv filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);

map<int64_t, set<string>> labels;
set<string> currentLabels;

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    auto csvData = LoadCSV(InputFilename);

    auto baseGraph = ProbabilityTransform(csvData);
    auto probabilityGraph = baseGraph;

    bool change = true;
    set<Kernel> kernels;
    while (change)
    {
        change = false;
        int priorSize = kernels.size();
        //start by adding new kernels
        for (int i = 0; i < probabilityGraph.WeightMatrix.size(); i++)
        {
            auto path = Dijkstra(probabilityGraph, i, i);
            if (!path.empty())
            {
                auto newKernel = Kernel();
                for (const auto &step : path)
                {
                    newKernel.Blocks.insert(probabilityGraph.IndexAlias[step].begin(), probabilityGraph.IndexAlias[step].end());
                }
                kernels.insert(newKernel);
                cout << "hi";
            }
        }
        if (priorSize != kernels.size())
        {
            change = true;
        }

        //now that we added new kernels, legalize them
        for (auto &kernel : kernels)
        {
            if (!kernel.IsLegal(baseGraph, kernels))
            {
                throw AtlasException("Unimplemented");
            }
        }
        //now rebuild the graph with kernels collapsed
        probabilityGraph = GraphCollapse(probabilityGraph, kernels);
    }

    nlohmann::json outputJson;
    int i = 0;
    for (const auto &kernel : kernels)
    {
        outputJson["Kernels"]["K" + to_string(i++)] = kernel.Blocks;
    }

    ofstream oStream(OutputFilename);
    oStream << outputJson;
    oStream.close();

    return EXIT_SUCCESS;
}