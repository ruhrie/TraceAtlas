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
// cl::opt<int> LogLevel("v", cl::desc("Logging level"), cl::value_desc("logging level"), cl::init(4));
// cl::opt<string> LogFile("l", cl::desc("Specify log filename"), cl::value_desc("log file"));


bool sortKey(string xin, string yin)
{
    std::cout<<"sort in " << std::endl;
    string pattern("@");
    size_t pos = xin.find(pattern);
    string x = xin.substr(pos+1, xin.size());
    int xValue = stoi(x, 0, 0);
    pos = yin.find(pattern);
    string y = yin.substr(pos+1, yin.size());
    int yValue = stoi(y, 0, 0);

    if (xValue== -1 && yValue == -1)
    {
        //std::cout<<"sort 1: " << WorkingSet::virAddr[xin][2]<< "%%% " << WorkingSet::virAddr[yin][2] << std::endl;
        return WorkingSet::virAddr[xin][2] < WorkingSet::virAddr[yin][2];
    }
    else
    {
        //std::cout<<"sort 2 " << std::endl;
        return xValue < yValue;
    } 	    
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    ProcessTrace(inputTrace, &WorkingSet::Process, "working set analysis", false);
    int keySize = WorkingSet::virAddr.size();
    vector<string> keyVector;
    
    for(map< string,  vector<uint64_t> > ::iterator it = WorkingSet::virAddr.begin(); it != WorkingSet::virAddr.end();++it)
    {
        keyVector.push_back(it->first);
    }

    std::sort(keyVector.begin(), keyVector.end(), sortKey);


    // alivenumber = inputSize
    // for time in range(0, timing+1):
    //     timeline = []
    //     reviseDic = []
    //     for addrPair in L:
    //         if virAddr[addrPair][1] > time:
    //             break
    //         if virAddr[addrPair][2]:
    //             if int(virAddr[addrPair][1]) <= time < int(virAddr[addrPair][2][-1]):
    //                 timeline.append(addrPair)
    //             elif time > int(virAddr[addrPair][2][-1]):
    //                 reviseDic.append(addrPair)
    //         else:
    //             if addrPair not in outputList:
    //                 outputList.append(addrPair)
    //     for i in reviseDic:
    //         del virAddr[i]
    //         L.remove(i)
    //     aliveTbl.append(timeline)
    //     outputWorking.append(outputList.__len__())
    //     output.append(timeline.__len__())

    //     timeline = []
    //     reviseDic = []
    //     for addrPair in Lin:
    //         if virAddrInput[addrPair][1][0] > time:
    //             break
    //         if time > int(virAddrInput[addrPair][1][-1]):
    //             reviseDic.append(addrPair)
    //     if Lin.__len__():
    //         alivenumber = alivenumber - reviseDic.__len__()
    //     else:
    //         alivenumber = 0
    //     inputWorking.append(alivenumber)
    //     for i in reviseDic:
    //         del virAddrInput[i]
    //         Lin.remove(i)
    //     if time%100 == 0:
    //         print("time progress:",time)


}