#pragma once
#include "cartographer/new/inc/GraphNode.h"
#include "cartographer/new/inc/Kernel.h"
#include "cartographer/new/inc/VKNode.h"
#include <exception>
#include <fstream>
#include <iostream>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>
#include <spdlog/spdlog.h>

void DebugExports(llvm::Module *, const std::string &);

inline void PrintVal(llvm::Value *val, bool print = true)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    if (print)
    {
        std::cout << str << "\n";
    }
}

inline void PrintVal(llvm::Metadata *val)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    std::cout << str << "\n";
}

inline void PrintVal(llvm::NamedMDNode *val)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    std::cout << str << "\n";
}

inline void PrintVal(llvm::Module *mod)
{
    llvm::AssemblyAnnotationWriter *write = new llvm::AssemblyAnnotationWriter();
    std::string str;
    llvm::raw_string_ostream rso(str);
    mod->print(rso, write);
    std::cout << str << "\n";
}

inline void PrintVal(llvm::Type *val)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    std::cout << str << "\n";
}

inline int PrintFile(llvm::Module *M, std::string &file, bool ASCIIFormat, bool Debug)
{
    try
    {
        if (ASCIIFormat || Debug)
        {
            // print human readable tik module to file
            auto *write = new llvm::AssemblyAnnotationWriter();
            std::string str;
            llvm::raw_string_ostream rso(str);
            std::filebuf f0;
            f0.open(file, std::ios::out);
            M->print(rso, write);
            std::ostream readableStream(&f0);
            readableStream << str;
            f0.close();
            if (Debug)
            {
                DebugExports(M, file);
                spdlog::info("Successfully injected debug symbols into tikSwap bitcode.");
                f0.open(file, std::ios::out);
                std::string str2;
                llvm::raw_string_ostream rso2(str2);
                M->print(rso2, write);
                std::ostream final(&f0);
                final << str2;
                f0.close();
            }
        }
        else
        {
            // non-human readable IR
            std::filebuf f;
            f.open(file, std::ios::out);
            std::ostream rawStream(&f);
            llvm::raw_os_ostream raw_stream(rawStream);
            WriteBitcodeToFile(*M, raw_stream);
        }
        spdlog::info("Successfully wrote tikSwap to file");
    }
    catch (std::exception &e)
    {
        spdlog::critical("Failed to open output file: " + file + ":\n" + e.what() + "\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

inline void PrintGraph(const std::set<TraceAtlas::Cartographer::GraphNode *, TraceAtlas::Cartographer::p_GNCompare> &nodes)
{
    for (const auto &node : nodes)
    {
        spdlog::info("Examining node " + std::to_string(node->NID));
        if (auto VKN = dynamic_cast<TraceAtlas::Cartographer::VKNode *>(node))
        {
            spdlog::info("This node is a virtual kernel pointing to ID " + std::to_string(VKN->kernel->KID));
        }
        std::string blocks;
        for (const auto &b : node->blocks)
        {
            blocks += std::to_string(b) + ",";
        }
        spdlog::info("This node contains blocks: " + blocks);
        std::string preds;
        for (auto pred : node->predecessors)
        {
            preds += std::to_string(pred);
            if (pred != *prev(node->predecessors.end()))
            {
                preds += ",";
            }
        }
        spdlog::info("Predecessors: " + preds);
        for (const auto &neighbor : node->neighbors)
        {
            spdlog::info("Neighbor " + std::to_string(neighbor.first) + " has instance count " + std::to_string(neighbor.second.first) + " and probability " + std::to_string(neighbor.second.second));
        }
        std::cout << std::endl;
    }
}