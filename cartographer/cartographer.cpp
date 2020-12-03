#include "AtlasUtil/Format.h"
#include "AtlasUtil/IO.h"
#include "AtlasUtil/Traces.h"
#include "TypeFour.h"
#include "TypeOne.h"
#include "TypeThree.h"
#include "TypeTwo.h"
#include "dot.h"
#include "profile.h"
#include <functional>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/CFG.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <string>
#include <tuple>

using namespace std;
using namespace llvm;

bool noProgressBar;
bool blocksLabeled = false;
map<int64_t, set<string>> blockLabelMap;
map<int64_t, BasicBlock *> blockMap;
set<int64_t> ValidBlocks;

cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
float threshold = 0;
cl::opt<int> hotThreshold("ht", cl::desc("The minimum instance count for a basic block to be a seed."), llvm::cl::init(512));
cl::opt<string> kernelFile("k", llvm::cl::desc("Specify output json name"), llvm::cl::value_desc("kernel filename"), llvm::cl::init("kernel.json"));
cl::opt<string> profileFile("p", llvm::cl::desc("Specify profile json name"), llvm::cl::value_desc("profile filename"));
cl::list<string> bitcodeFiles("b", llvm::cl::desc("Specify bitcode name"), llvm::cl::value_desc("bitcode filename"), llvm::cl::Required, cl::OneOrMore);
cl::opt<bool> label("L", llvm::cl::desc("ExportLabel"), llvm::cl::value_desc("Export library label"), cl::init(false));
cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));
cl::opt<int> LogLevel("v", cl::desc("Logging level"), cl::value_desc("logging level"), cl::init(4));
cl::opt<string> LogFile("l", cl::desc("Specify log filename"), cl::value_desc("log file"));
cl::opt<string> DotFile("d", cl::desc("Specify dot filename"), cl::value_desc("dot file"));
cl::opt<string> DumpFile("D", cl::desc("Block relationship file"), cl::value_desc("Relationship file"));
llvm::cl::opt<bool> Debug("db", llvm::cl::desc("Debug output"), llvm::cl::value_desc("Export debug information to utput file"));
cl::opt<bool> Preformat("pf", llvm::cl::desc("Bitcode is preformatted"), llvm::cl::value_desc("Bitcode is preformatted"));

void Dump(const string &dump, std::vector<Module *> &Ms)
{
    nlohmann::json dumpJson;
    for (auto &M : Ms)
    {
        for (auto &mi : *M)
        {
            for (auto &fi : mi)
            {
                auto BB = cast<BasicBlock>(&fi);
                auto id = GetBlockID(BB);
                set<int64_t> blocks;
                for (auto suc : successors(BB))
                {
                    blocks.insert(GetBlockID(suc));
                }

                for (auto &bi : fi)
                {
                    if (auto i = dyn_cast<CallBase>(&bi))
                    {
                        auto F = i->getCalledFunction();
                        if (F != nullptr && !F->empty())
                        {
                            auto entry = &F->getEntryBlock();
                            blocks.insert(GetBlockID(entry));
                        }
                    }
                }

                auto term = BB->getTerminator();
                if (auto ret = dyn_cast<ReturnInst>(term))
                {
                    auto F = BB->getParent();
                    for (auto user : F->users())
                    {
                        if (auto i = dyn_cast<CallBase>(user))
                        {
                            blocks.insert(GetBlockID(i->getParent()));
                        }
                    }
                }
                else if (auto res = dyn_cast<ResumeInst>(term))
                {
                    auto F = BB->getParent();
                    for (auto user : F->users())
                    {
                        if (auto i = dyn_cast<CallBase>(user))
                        {
                            blocks.insert(GetBlockID(i->getParent()));
                        }
                    }
                }

                dumpJson[to_string(id)] = blocks;
            }
        }
    }
    ofstream oStream(dump);
    oStream << dumpJson;
    oStream.close();
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    threshold = 1.0f - 2.0f / (0.9f * hotThreshold);
    noProgressBar = noBar;

    if (!LogFile.empty())
    {
        auto file_logger = spdlog::basic_logger_mt("cartographer_logger", LogFile);
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
            break;
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
    spdlog::trace("Set logging level");
    vector<unique_ptr<Module>> bitcodes = LoadBitcodes(bitcodeFiles);
    vector<Module *> bitcodePtrs(bitcodes.size());
    uint64_t i = 0;
    for (const auto &b : bitcodes)
    {
        bitcodePtrs[i++] = b.get();
    }

    if (!Preformat)
    {
        for (const auto &bitcode : bitcodes)
        {
            Format(bitcode.get());
        }
        spdlog::trace("Succesfully formatted IR");
    }
    else
    {
        spdlog::trace("Skipped formatting preannotated IR");
    }

    //build the blockMap
    for (auto &M : bitcodes)
    {
        for (auto &mi : *M)
        {
            for (auto fi = mi.begin(); fi != mi.end(); fi++)
            {
                auto *bb = cast<BasicBlock>(fi);
                int64_t id = GetBlockID(bb);
                blockMap[id] = bb;
            }
        }
    }

    spdlog::trace("Finished building block map");

    try
    {
        spdlog::info("Started analysis");
        ProcessTrace(inputTrace, &TypeOne::Process, "Detecting type 1 kernels", noBar);
        auto type1Kernels = TypeOne::Get();
        spdlog::info("Detected " + to_string(type1Kernels.size()) + " type 1 kernels");
        vector<int64_t> blockIndeces;
        for (auto &[block, count] : TypeOne::blockCount)
        {
            if (count != 0)
            {
                ValidBlocks.insert(block);
                blockIndeces.push_back(block);
            }
        }
        
        TypeTwo::Setup(bitcodePtrs, type1Kernels);
        ProcessTrace(inputTrace, &TypeTwo::Process, "Detecting type 2 kernels", noBar);
        auto type2Kernels = TypeTwo::Get();
        spdlog::info("Detected " + to_string(type2Kernels.size()) + " type 2 kernels");

        TypeTwo::Setup(bitcodePtrs, type2Kernels);
        ProcessTrace(inputTrace, &TypeTwo::Process, "Detecting type 2.5 kernels", noBar);
        auto type25Kernels = TypeTwo::Get();
        spdlog::info("Detected " + to_string(type25Kernels.size()) + " type 2.5 kernels");

        auto type3Kernels = TypeThree::Process(type25Kernels);
        spdlog::info("Detected " + to_string(type3Kernels.size()) + " type 3 kernels");

        auto type4Kernels = TypeFour::Process(type3Kernels);
        spdlog::info("Detected " + to_string(type4Kernels.size()) + " type 4 kernels");

        auto type35Kernels = TypeThree::Process(type4Kernels);
        spdlog::info("Detected " + to_string(type35Kernels.size()) + " type 3.5 kernels");

        map<int, set<int64_t>> finalResult;
        int j = 0;
        for (const auto &set : type35Kernels)
        {
            if (!set.empty())
            {
                finalResult[j++] = set;
            }
        }

        nlohmann::json outputJson;
        for (const auto &key : finalResult)
        {
            if (label)
            {
                string strLabel;
                bool first = true;
                set<string> labels;
                for (auto block : key.second)
                {
                    labels.insert(blockLabelMap[block].begin(), blockLabelMap[block].end());
                }
                for (const auto &entry : labels)
                {
                    if (entry.empty())
                    {
                        continue;
                    }
                    if (first)
                    {
                        first = false;
                    }
                    else
                    {
                        strLabel += ";";
                    }
                    strLabel += entry;
                }
                outputJson["Kernels"][to_string(key.first)]["Blocks"] = key.second;
                outputJson["Kernels"][to_string(key.first)]["Label"] = strLabel;
            }
            else
            {
                outputJson["Kernels"][to_string(key.first)]["Blocks"] = key.second;
            }
        }
        outputJson["ValidBlocks"] = ValidBlocks;
        map<string, set<int32_t>> calledIndeces;
        for (auto blockId : blockIndeces)
        {
            auto calledBlocks = TypeTwo::blockCaller[blockId];
            calledIndeces[to_string(blockId)] = calledBlocks;
        }

        outputJson["BlockCallers"] = calledIndeces;

        if (Debug)
        {
            //block counts
            {
                map<string, uint64_t> t;
                for (const auto &a : TypeOne::blockCount)
                {
                    t[to_string(a.first)] = a.second;
                }
                outputJson["BlockCounts"] = t;
            }

            //type 1
            {
                map<string, set<int64_t>> t;
                int j = 0;
                for (const auto &set : type1Kernels)
                {
                    if (!set.empty())
                    {
                        t[to_string(j++)] = set;
                    }
                }
                outputJson["TypeOne"] = t;
            }
            //type 2
            {
                map<string, set<int64_t>> t;
                int j = 0;
                for (const auto &set : type2Kernels)
                {
                    if (!set.empty())
                    {
                        t[to_string(j++)] = set;
                    }
                }
                outputJson["TypeTwo"] = t;
            }
            //type 2.5
            {
                map<string, set<int64_t>> t;
                int j = 0;
                for (const auto &set : type25Kernels)
                {
                    if (!set.empty())
                    {
                        t[to_string(j++)] = set;
                    }
                }
                outputJson["TypeTwoFive"] = t;
            }
            //type 3
            {
                map<string, set<int64_t>> t;
                int j = 0;
                for (const auto &set : type3Kernels)
                {
                    if (!set.empty())
                    {
                        t[to_string(j++)] = set;
                    }
                }
                outputJson["TypeThree"] = t;
            }
            //type 4
            {
                map<string, set<int64_t>> t;
                int j = 0;
                for (const auto &set : type4Kernels)
                {
                    if (!set.empty())
                    {
                        t[to_string(j++)] = set;
                    }
                }
                outputJson["TypeFour"] = t;
            }
            //type 3.5
            {
                map<string, set<int64_t>> t;
                int j = 0;
                for (const auto &set : type35Kernels)
                {
                    if (!set.empty())
                    {
                        t[to_string(j++)] = set;
                    }
                }
                outputJson["TypeThreeFive"] = t;
            }
        }
        ofstream oStream(kernelFile);
        oStream << outputJson;
        oStream.close();
        if (!profileFile.empty())
        {
            nlohmann::json prof = ProfileKernels(finalResult, bitcodePtrs);
            ofstream pStream(profileFile);
            pStream << prof;
            pStream.close();
        }
        if (!DotFile.empty())
        {
            ofstream dStream(DotFile);
            auto graph = GenerateDot(type35Kernels);
            dStream << graph << "\n";
            dStream.close();
        }
        if (!DumpFile.empty())
        {
            Dump(DumpFile, bitcodePtrs);
        }
    }
    catch (AtlasException e)
    {
        spdlog::critical("Failed to analyze trace");
        spdlog::critical(e.what());
        return EXIT_FAILURE;
    }
    catch (int e)
    {
        spdlog::critical("Failed to analyze trace");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
