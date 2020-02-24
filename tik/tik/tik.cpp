#include "tik/tik.h"
#include "AtlasUtil/Annotate.h"
#include "tik/Exceptions.h"
#include "tik/Util.h"
#include <fstream>
#include <iostream>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <string>

using namespace std;
using namespace llvm;

enum Filetype
{
    LLVM,
    DPDA
};

llvm::Module *TikModule;
std::map<int, Kernel *> KernelMap;
std::map<llvm::Function *, Kernel *> KfMap;
cl::opt<string> JsonFile("j", cl::desc("Specify input json filename"), cl::value_desc("json filename"));
cl::opt<string> OutputFile("o", cl::desc("Specify output filename"), cl::value_desc("output filename"));
cl::opt<string> InputFile(cl::Positional, cl::Required, cl::desc("<input file>"));
cl::opt<Filetype> InputType("t", cl::desc("Choose input file type"),
                            cl::values(
                                clEnumVal(LLVM, "LLVM IR"),
                                clEnumVal(DPDA, "DPDA")),
                            cl::init(LLVM));
cl::opt<string> OutputType("f", cl::desc("Specify output file format. Can be either JSON or LLVM"), cl::value_desc("format"));
cl::opt<bool> ASCIIFormat("S", cl::desc("output json as human-readable ASCII text"));
cl::opt<string> LogFile("l", cl::desc("Specify log filename"), cl::value_desc("log file"));
cl::opt<int> LogLevel("v", cl::desc("Logging level"), cl::value_desc("logging level"), cl::init(4));

int main(int argc, char *argv[])
{
    bool error = false;
    cl::ParseCommandLineOptions(argc, argv);

    if (!LogFile.empty())
    {
        auto file_logger = spdlog::basic_logger_mt("tik_logger", LogFile);
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
    ifstream inputJson;
    nlohmann::json j;
    try
    {
        inputJson.open(JsonFile);
        inputJson >> j;
        inputJson.close();
    }
    catch (TikException &e)
    {
        std::cerr << "Couldn't open input json file: " << JsonFile << "\n";
        std::cerr << e.what() << '\n';
        spdlog::critical("Failed to open kernel file: " + JsonFile);
        return EXIT_FAILURE;
    }
    spdlog::info("Found " + to_string(j.size()) + " kernels in the kernel file");

    map<string, vector<int>> kernels;

    for (auto &[key, value] : j.items())
    {
        string index = key;
        nlohmann::json kernel;
        if(!value[0].empty() && value[0].is_array())
        {
            //embedded layout
            kernel = value[0];
        }
        else
        {  
            kernel = value;
        }
        
        //std::cout << value;
        //vector<int> kernel = value;
        kernels[index] = kernel.get<vector<int>>();
    }

    map<string, vector<string>> childParentMapping;

    for (auto element : kernels)
    {
        for (auto comparison : kernels)
        {
            if (element.first != comparison.first)
            {
                if (element.second < comparison.second)
                {
                    vector<int> i;
                    set_intersection(element.second.begin(), element.second.end(), comparison.second.begin(), comparison.second.end(), back_inserter(i));
                    if (i == comparison.second)
                    {
                        childParentMapping[element.first].push_back(comparison.first);
                    }
                }
            }
        }
    }

    //load the llvm file
    LLVMContext context;
    SMDiagnostic smerror;
    unique_ptr<Module> sourceBitcode;
    try
    {
        sourceBitcode = parseIRFile(InputFile, smerror, context);
    }
    catch (TikException &e)
    {
        std::cerr << "Couldn't open input bitcode file: " << InputFile << "\n";
        std::cerr << e.what() << '\n';
        spdlog::critical("Failed to open source bitcode: " + InputFile);
        return EXIT_FAILURE;
    }

    //annotate it with the same algorithm used in the tracer
    Annotate(sourceBitcode.get());

    TikModule = new Module(InputFile, context);

    //we now process all kernels who have no children and then remove them as we go
    std::vector<Kernel *> results;

    bool change = true;
    set<vector<int>> failedKernels;
    while (change)
    {
        change = false;
        for (auto kernel : kernels)
        {
            if (failedKernels.find(kernel.second) != failedKernels.end())
            {
                continue;
            }
            if (childParentMapping.find(kernel.first) == childParentMapping.end())
            {
                try
                {
                    //this kernel has no unexplained parents
                    Kernel *kern = new Kernel(kernel.second, sourceBitcode.get(), kernel.first);
                    KfMap[kern->KernelFunction] = kern;
                    //so we remove its blocks from all parents
                    vector<string> toRemove;
                    for (auto child : childParentMapping)
                    {
                        auto loc = find(child.second.begin(), child.second.end(), kernel.first);
                        if (loc != child.second.end())
                        {
                            child.second.erase(loc);
                            if (child.second.size() == 0)
                            {
                                toRemove.push_back(child.first);
                            }
                        }
                    }
                    //if necessary remove the entry from the map
                    for (auto r : toRemove)
                    {
                        auto it = childParentMapping.find(r);
                        childParentMapping.erase(it);
                    }
                    //publish our result
                    results.push_back(kern);
                    change = true;
                    for (auto block : kernel.second)
                    {
                        if (KernelMap.find(block) == KernelMap.end())
                        {
                            KernelMap[block] = kern;
                        }
                    }
                    //and remove it from kernels
                    auto it = find(kernels.begin(), kernels.end(), kernel);
                    kernels.erase(it);
                    spdlog::info("Succesfully converted kernel: " + kernel.first);
                    //and restart the iterator to ensure cohesion
                    break;
                }
                catch (TikException &e)
                {
                    failedKernels.insert(kernel.second);
                    std::cerr << "Failed to convert kernel to tik"
                              << "\n";
                    std::cerr << e.what() << '\n';
                    error = true;
                    spdlog::error("Failed to convert kernel: " + kernel.first);
                }
            }
        }
    }

    // writing part

    try
    {
        if (OutputType == "JSON")
        {
            nlohmann::json finalJson;
            for (Kernel *kern : results)
            {
                finalJson["Kernels"][kern->Name] = kern->GetJson();
            }
            ofstream oStream(OutputFile);
            oStream << finalJson;
            oStream.close();
        }
        else
        {
            if (ASCIIFormat)
            {
                // print human readable tik module to file
                AssemblyAnnotationWriter *write = new llvm::AssemblyAnnotationWriter();
                std::string str;
                llvm::raw_string_ostream rso(str);
                std::filebuf f0;
                f0.open(OutputFile, std::ios::out);
                TikModule->print(rso, write);
                std::ostream readableStream(&f0);
                readableStream << str;
                f0.close();
            }
            else
            {
                // non-human readable IR
                std::filebuf f;
                f.open(OutputFile, std::ios::out);
                std::ostream rawStream(&f);
                raw_os_ostream raw_stream(rawStream);
                WriteBitcodeToFile(*TikModule, raw_stream);
            }
        }
        spdlog::info("Succesfully wrote tik to file");
    }
    catch (TikException &e)
    {
        std::cerr << "Failed to open output file: " << OutputFile << "\n";
        std::cerr << e.what() << '\n';
        spdlog::critical("Failed to write tik to output file: " + OutputFile);
        return EXIT_FAILURE;
    }
    if (error)
    {
        return EXIT_FAILURE;
    }
    else
    {
        return EXIT_SUCCESS;
    }
}
