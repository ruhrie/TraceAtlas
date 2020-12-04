#include "AtlasUtil/Format.h"
#include "AtlasUtil/Graph/Dijkstra.h"
#include "AtlasUtil/Graph/Graph.h"
#include "AtlasUtil/Graph/GraphTransforms.h"
#include "AtlasUtil/Graph/Kernel.h"
#include "AtlasUtil/IO.h"
#include "AtlasUtil/Logging.h"
#include "AtlasUtil/Traces.h"
#include <algorithm>
#include <cfloat>
#include <fstream>
#include <llvm/Support/CommandLine.h>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

using namespace llvm;
using namespace std;

cl::opt<std::string> InputFilename("i", cl::desc("Specify csv file"), cl::value_desc("csv filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);
cl::opt<int> LogLevel("v", cl::desc("Logging level"), cl::value_desc("logging level"), cl::init(4));
cl::opt<string> LogFile("l", cl::desc("Specify log filename"), cl::value_desc("log file"));

map<int64_t, set<string>> labels;
set<string> currentLabels;

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);

    SetupLogger(LogFile, LogLevel);

    spdlog::trace("Loading csv");
    auto csvData = LoadCSV(InputFilename);

    spdlog::trace("Building graph");
    auto baseGraph = ProbabilityTransform(csvData);
    auto probabilityGraph = baseGraph;

    bool change = true;
    set<GraphKernel> kernels;
    int count = 0;
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
                auto newKernel = GraphKernel();
                for (const auto &step : path)
                {
                    newKernel.Blocks.insert(probabilityGraph.IndexAlias[step].begin(), probabilityGraph.IndexAlias[step].end());
                }
                kernels.insert(newKernel);
            }
        }
        if (priorSize != kernels.size())
        {
            change = true;
        }

        //now that we added new kernels, legalize them
        //step 1: grow kernels to have the most probable paths
        bool change = true;
        bool fuse = false;
        set<GraphKernel> stepOneKernels;
        while (change)
        {
            change = false;
            GraphKernel fuseA;
            GraphKernel fuseB;
            bool fuse = false;
            for (GraphKernel kernel : kernels)
            {
                while (true)
                {
                    auto legality = kernel.IsLegal(baseGraph, kernels, probabilityGraph);
                    if (legality == Legality::Legal || legality == Legality::RuleTwo)
                    {
                        break;
                    }
                    float minScore = FLT_MAX;
                    GraphKernel currentCandidate;
                    for (const auto &cComp : kernels)
                    {
                        if (kernel != cComp)
                        {
                            float score = kernel.ScoreSimilarity(cComp, csvData, baseGraph);
                            if (score != -1 && score < minScore)
                            {
                                minScore = score;
                                currentCandidate = cComp;
                            }
                        }
                    }
                    if (minScore == FLT_MAX)
                    {
                        throw AtlasException("Unimplemented");
                    }
                    kernel.Blocks.insert(currentCandidate.Blocks.begin(), currentCandidate.Blocks.end());
                }
                stepOneKernels.insert(kernel);
            }
        }
        spdlog::trace("Discovered {0} kernels during step 1, pass {1}", stepOneKernels.size(), count);
        //step 2: fuse kernels that paritally overlap
        set<GraphKernel> stepTwoKernels;
        for (const GraphKernel &kernel : stepOneKernels)
        {
            while (true)
            {
                auto legality = kernel.IsLegal(baseGraph, stepOneKernels, probabilityGraph);
                if (legality == Legality::Legal)
                {
                    break;
                }
                throw AtlasException("Unimplemented");
            }
            stepTwoKernels.insert(kernel);
        }

        spdlog::trace("Discovered {0} kernels during step 2, pass {1}", stepTwoKernels.size(), count);
        //step 3: sanity check to make sure they are all legal
        for (const GraphKernel &kernel : stepTwoKernels)
        {
            auto legality = kernel.IsLegal(baseGraph, stepTwoKernels, probabilityGraph);
            if (legality != Legality::Legal)
            {
                throw AtlasException("Illegal kernel formed");
            }
        }

        spdlog::trace("Discovered {0} kernels during pass {1}", stepTwoKernels.size(), count++);
        //finally replace the prior kernels
        kernels.clear();
        kernels.insert(stepTwoKernels.begin(), stepTwoKernels.end());

        spdlog::trace("Collapsing kernel graph");
        //now rebuild the graph with kernels collapsed
        probabilityGraph = GraphCollapse(probabilityGraph, kernels);
    }

    spdlog::info("Finished discovering {0} kernels", kernels.size());

    nlohmann::json outputJson;
    int i = 0;
    for (const auto &kernel : kernels)
    {
        outputJson["Kernels"]["K" + to_string(i++)]["Blocks"] = kernel.Blocks;
    }

    spdlog::trace("Writing kernels to {0}", OutputFilename);
    ofstream oStream(OutputFilename);
    oStream << outputJson;
    oStream.close();
    return EXIT_SUCCESS;
}