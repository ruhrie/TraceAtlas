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

    auto csvData = LoadBIN(InputFilename);
    auto compressedGraph = CompressGraph(csvData);
    auto baseGraph = ProbabilityTransform(compressedGraph);
    auto probabilityGraph = baseGraph;

    bool change = true;
    set<GraphKernel> kernels;
    int count = 0;
    while (change)
    {
        change = false;
        int priorSize = kernels.size();
        spdlog::info("Finding base kernels, pass {0}", count);
        //start by adding new kernels
        for (int i = 0; i < probabilityGraph.WeightMatrix.size(); i++)
        {
            spdlog::trace("{0} of {1}", i, probabilityGraph.WeightMatrix.size());
            // find minimum probability cycle for i and if one exists make a kernel out of it
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
        // This step legalizes kernels that fail rules 1, 3 and 4
        //now that we added new kernels, legalize them
        //step 1: grow kernels to have the most probable paths
        spdlog::info("Stage one kernel refinements, pass {0}", count);
        bool fuse = false;
        set<GraphKernel> stepOneKernels;
        int i = 0;
        for (GraphKernel kernel : kernels)
        {
            try
            {
                spdlog::trace("{0} of {1}", i++, kernels.size());
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
                            // if this kernel and the compare kernel, when fused together, are strongly connected and are more similar than any other combo, set it as the merge candidate
                            if (score != -1 && score < minScore)
                            {
                                minScore = score;
                                currentCandidate = cComp;
                            }
                        }
                    }
                    if (minScore == FLT_MAX)
                    {
                        // this means that a kernel, when fused with any other kernel, does not form a strongly connected kernel
                        // therefore this kernel does not satisfy one or more of rules 1, 3 and 4
                        if (legality == Legality::RuleOne)
                        {
                            throw AtlasException("Base kernel does not satisfy rule 1.");
                        }
                        else if (legality == Legality::RuleThree)
                        {
                            throw AtlasException("Base kernel does not satisfy rule 3.")
                        }
                        else if (legality == Legality::RuleFour)
                        {
                            throw AtlasException("Base kernel does not satisfy rule 4.");
                        }
                        else
                        {
                            throw AtlasException("Base kernel exception unclear.");
                        }
                    }
                    kernel.Blocks.insert(currentCandidate.Blocks.begin(), currentCandidate.Blocks.end());
                }
                stepOneKernels.insert(kernel);
            }
            catch (AtlasException &e)
            {
                spdlog::error(e.what());
            }
        }
        if (priorSize != stepOneKernels.size())
        {
            change = true;
        }
        spdlog::info("Discovered {0} kernels during step 1, pass {1}", stepOneKernels.size(), count);
        //step 2: fuse kernels that paritally overlap
        spdlog::info("Stage two kernel refinements, pass {0}", count);
        set<GraphKernel> stepTwoKernels;
        i = 0;
        for (const GraphKernel &kernel : stepOneKernels)
        {
            try
            {
                spdlog::trace("{0} of {1}", i++, stepOneKernels.size());
                while (true)
                {
                    auto legality = kernel.IsLegal(baseGraph, stepOneKernels, probabilityGraph);
                    if (legality == Legality::Legal)
                    {
                        break;
                    }
                    spdlog::error("Failed to convert a kernel");
                    throw AtlasException("Type 1 kernel could not be separated from another type 1 kernel.");
                }
                stepTwoKernels.insert(kernel);
            }
            catch (AtlasException &e)
            {
                spdlog::error(e.what());
            }
        }

        spdlog::info("Discovered {0} kernels during step 2, pass {1}", stepTwoKernels.size(), count);
        //step 3: sanity check to make sure they are all legal
        i = 0;
        spdlog::info("Performing sanity check on kernels, pass {0}", count);
        for (const GraphKernel &kernel : stepTwoKernels)
        {
            try
            {
                spdlog::trace("{0} of {1}", i++, stepTwoKernels.size());
                auto legality = kernel.IsLegal(baseGraph, stepTwoKernels, probabilityGraph);
                if (legality != Legality::Legal)
                {
                    throw AtlasException("Type 2 kernel was not legal");
                }
            }
            catch (AtlasException &e)
            {
                spdlog::error(e.what());
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