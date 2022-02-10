#include "AtlasUtil/Traces.h"
#include "AtlasUtil/Annotate.h"
#include <algorithm>
#include <fstream>

#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Analysis/DependenceAnalysis.h>
#include "llvm/ADT/SmallVector.h"


using namespace llvm;
using namespace std;

cl::opt<std::string> InputFilename("i", cl::desc("Specify input trace"), cl::value_desc("trace filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);
llvm::cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));
llvm::cl::opt<string> bitcodeFile("b", llvm::cl::desc("Specify bitcode name"), llvm::cl::value_desc("bitcode filename"), llvm::cl::Required);

map<int64_t, set<string>> labels;
string currentLabel = "-1";
map<string,set<int64_t>> labelsToblock;
int64_t currentblock;

//== for DAG node map
set <int64_t> legalBBs;
typedef struct nodeInfo
{
    string label;
    set <int64_t> bbs;
} nodeInfo;



map<string, int64_t> kernellabelID;


map<int64_t,nodeInfo> nodeKiidMap;
int64_t kernelIdCounter = 0;
int64_t kernelInstanceIdCounter = 0;
set <int64_t> kernelInstanceBBs;


// check if the previous node is a legal non kernel
// case 1 back to back labeled kernels
// case 2 no load and store in the interval? 
bool CheckPrevKernelNode()
{
    // no blocks
    if (kernelInstanceBBs.size() ==0)
    {
        return false;
    }
    // to do add no load/store things

    for(auto i :kernelInstanceBBs)
    {
        if (legalBBs.find(i)!= legalBBs.end())
        {
            return true;
        }
    }
    return false;
}

// assume no nested kernels
void Process(string &key, string &value)
{
    //kernel enter concludes the previous node, kernel or non-kernel
    if (key == "KernelEnter")
    {   
        if(CheckPrevKernelNode())
        {
            nodeInfo newNode = nodeInfo{currentLabel,kernelInstanceBBs};
            nodeKiidMap[kernelInstanceIdCounter] = newNode;

            kernelInstanceIdCounter++;   
            currentLabel = value;
            kernelInstanceBBs.clear();
        }
        else
        {
            currentLabel = value;
        }
        
        
    }
    //kernel exit concludes the previous kernel node
    else if (key == "KernelExit")
    {
    
        labelsToblock[currentLabel].insert(currentblock);

        // extra illegal bbs outside the kernel
        for (auto i :kernelInstanceBBs)
        {
            labelsToblock[currentLabel].insert(i);
        }

        // adding nodes for kernels
        nodeInfo newNode = nodeInfo{currentLabel,kernelInstanceBBs};
        nodeKiidMap[kernelInstanceIdCounter] = newNode;
        currentLabel = "-1";
        kernelInstanceBBs.clear();
        kernelInstanceIdCounter++;
    }

    else if (key == "BBExit")
    {
        int64_t block = stol(value, nullptr, 0);
        if (currentLabel != "-1" && !currentLabel.empty())
        {
            labelsToblock[currentLabel].insert(block);
        }
    }
    else if (key == "BBEnter")
    {     
       currentblock = stol(value, nullptr, 0);
       kernelInstanceBBs.insert(currentblock);
    }
}

void GetLegalBBs()
{
    LLVMContext context;
    SMDiagnostic smerror;
    unique_ptr<Module> sourceBitcode;
    sourceBitcode = parseIRFile(bitcodeFile, smerror, context);
    Module *M = sourceBitcode.get();
    Annotate(M);
    for (auto &mi : *M)
    {
        for (auto fi = mi.begin(); fi != mi.end(); fi++)
        {
            auto *bb = cast<BasicBlock>(fi);
            auto dl = bb->getModule()->getDataLayout();
            int64_t id = GetBlockID(bb);
            int64_t instIndex = 0;
            for (auto bi = fi->begin(); bi != fi->end(); bi++)
            {
                if (auto *inst = dyn_cast<LoadInst>(bi))
                {
                    legalBBs.insert(id);
                    break;
                }
                else if (auto *inst = dyn_cast<StoreInst>(bi))
                {
                    legalBBs.insert(id);
                    break;
                }
                // else if (isa<CallInst>(&(*bi)) || isa<InvokeInst>(&(*bi)))
                // {
                //     legalBBs.insert(id);
                //     break;
                // }
                else if (auto *inst = dyn_cast<AllocaInst>(bi))
                {
                    legalBBs.insert(id);
                    break;
                }
            }
        }
    }

}
int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);

    GetLegalBBs();

    ProcessTrace(InputFilename, Process, "Generating JR", noBar);
    if(CheckPrevKernelNode())
    {
        nodeInfo newNode = nodeInfo{currentLabel,kernelInstanceBBs};
        nodeKiidMap[kernelInstanceIdCounter] = newNode;
    }

    std::ofstream file;
    nlohmann::json jOut;
 
    int count = 0;
    for (auto i : labelsToblock)
    {
        jOut["Kernels"][to_string(count)]["Blocks"] = i.second;
        jOut["Kernels"][to_string(count)]["Label"] = i.first;
        count++;
    }

    for (auto i : nodeKiidMap)
    {
        jOut["Control"][to_string(i.first)]["Blocks"] = i.second.bbs;
        jOut["Control"][to_string(i.first)]["Label"] = i.second.label;
    }

    // jOut["legalBBs"] = legalBBs;
  

    file.open(OutputFilename);
    file << std::setw(4) << jOut << std::endl;
    file.close();
}