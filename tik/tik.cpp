#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "tik/CartographerKernel.h"
#include "tik/Header.h"
#include "tik/Util.h"
#include "tik/libtik.h"
#include <fstream>
#include <iostream>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
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
using namespace TraceAtlas::tik;

enum Filetype
{
    LLVM,
    DPDA
};

set<int64_t> ValidBlocks;
cl::opt<string> JsonFile("j", cl::desc("Specify input json filename"), cl::value_desc("json filename"));
cl::opt<string> OutputFile("o", cl::desc("Specify output filename"), cl::value_desc("output filename"));
cl::opt<string> InputFile(cl::Positional, cl::Required, cl::desc("<input file>"));
cl::opt<Filetype> InputType("t", cl::desc("Choose input file type"),
                            cl::values(
                                clEnumVal(LLVM, "LLVM IR"),
                                clEnumVal(DPDA, "DPDA")),
                            cl::init(LLVM));
cl::opt<string> OutputType("f", cl::desc("Specify output file format. Can be LLVM"), cl::value_desc("format"));
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
    catch (exception &e)
    {
        std::cerr << "Couldn't open input json file: " << JsonFile << "\n";
        std::cerr << e.what() << '\n';
        spdlog::critical("Failed to open kernel file: " + JsonFile);
        return EXIT_FAILURE;
    }
    spdlog::info("Found " + to_string(j["Kernels"].size()) + " kernels in the kernel file");

    map<string, vector<int64_t>> kernels;

    for (auto &[k, l] : j["Kernels"].items())
    {
        string index = k;
        nlohmann::json kernel = l["Blocks"];
        kernels[index] = kernel.get<vector<int64_t>>();
    }
    ValidBlocks = j["ValidBlocks"].get<set<int64_t>>();

    map<string, vector<string>> childParentMapping;

    for (auto element : kernels)
    {
        for (auto comparison : kernels)
        {
            if (element.first != comparison.first)
            {
                if (element.second < comparison.second)
                {
                    vector<int64_t> i;
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
    catch (exception &e)
    {
        std::cerr << "Couldn't open input bitcode file: " << InputFile << "\n";
        std::cerr << e.what() << '\n';
        spdlog::critical("Failed to open source bitcode: " + InputFile);
        return EXIT_FAILURE;
    }

    if (sourceBitcode == nullptr)
    {
        std::cerr << "Couldn't open input bitcode file: " << InputFile << "\n";
        spdlog::critical("Failed to open source bitcode: " + InputFile);
        return EXIT_FAILURE;
    }

    Module *base = sourceBitcode.get();

    CleanModule(base);

    //annotate it with the same algorithm used in the tracer
    Annotate(base);

    TikModule = new Module(InputFile, context);
    TikModule->setDataLayout(sourceBitcode->getDataLayout());
    TikModule->setTargetTriple(sourceBitcode->getTargetTriple());

    //we now process all kernels who have no children and then remove them as we go
    std::vector<shared_ptr<Kernel>> results;

    bool change = true;
    set<vector<int64_t>> failedKernels;
    while (change)
    {
        change = false;
        for (const auto &kernel : kernels)
        {
            if (failedKernels.find(kernel.second) != failedKernels.end())
            {
                continue;
            }
            if (childParentMapping.find(kernel.first) == childParentMapping.end())
            {
                //this kernel has no unexplained parents
                auto kern = make_shared<CartographerKernel>(kernel.second, sourceBitcode.get(), kernel.first);
                if (!kern->Valid)
                {
                    failedKernels.insert(kernel.second);
                    error = true;
                    spdlog::error("Failed to convert kernel: " + kernel.first);
                }
                else
                {
                    KfMap[kern->KernelFunction] = kern;
                    //so we remove its blocks from all parents
                    vector<string> toRemove;
                    for (auto child : childParentMapping)
                    {
                        auto loc = find(child.second.begin(), child.second.end(), kernel.first);
                        if (loc != child.second.end())
                        {
                            child.second.erase(loc);
                            if (child.second.empty())
                            {
                                toRemove.push_back(child.first);
                            }
                        }
                    }
                    //if necessary remove the entry from the map
                    for (const auto &r : toRemove)
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
                    spdlog::info("Successfully converted kernel: " + kernel.first);
                    //and restart the iterator to ensure cohesion
                    break;
                }
            }
        }
    }

    // generate a C header file declaring each tik function
    std::string headerFile = "\n// Auto-generated header for the tik representations of " + InputFile + "\n";
    headerFile += "#include <stdint.h>\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n";
    // insert all structures in the tik module and convert them
    std::set<llvm::StructType *> AllStructures;
    headerFile += GetTikStructures(results, AllStructures);
    for (const auto &kernel : results)
    {
        headerFile += "\n" + kernel->GetHeaderDeclaration(AllStructures);
    }
    if (VectorsUsed)
    {
        std::string stdIntHeader = "\n#include <stdint.h>\n";
        std::string vectorHeader = "\n#include <immintrin.h>";
        headerFile.insert(headerFile.find(stdIntHeader), vectorHeader);
    }
    headerFile += "\n#ifdef __cplusplus\n}\n#endif\n";
    // write the header file
    std::ofstream header;
    header.open(OutputFile + ".h");
    header << headerFile;
    header.close();

    //verify the module
    std::string str;
    llvm::raw_string_ostream rso(str);
#ifdef DEBUG
    bool broken = verifyModule(*TikModule, &rso);
#else
    bool broken = verifyModule(*TikModule);
#endif
    if (broken)
    {
#ifdef DEBUG
        auto err = rso.str();
        spdlog::critical("Tik Module Corrupted: " + err);
        error = true;
#else
        spdlog::critical("Tik Module Corrupted");
        return EXIT_FAILURE;
#endif
    }

    // writing part
    try
    {
        if (ASCIIFormat)
        {
            // print human readable tik module to file
            auto *write = new llvm::AssemblyAnnotationWriter();
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

        spdlog::info("Successfully wrote tik to file");
    }
    catch (exception &e)
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
    return EXIT_SUCCESS;
}
