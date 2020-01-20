#include "EncodeExtract.h"
#include "EncodeDetect.h"
#include "cartographer.h"
#include <algorithm>
#include <assert.h>
#include <fstream>
#include <functional>
#include <indicators/progress_bar.hpp>
#include <iostream>
#include <list>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdlib.h>
#include <zlib.h>

#define BLOCK_SIZE 4096

using namespace std;
using namespace llvm;

std::map<int, std::set<int>> ExtractKernels(std::string sourceFile, std::vector<std::set<int>> kernels, Module *bitcode)
{
    indicators::ProgressBar bar;
    int previousCount = 0;
    if (!noProgressBar)
    {
        bar.set_prefix_text("Detecting type 2 kernels");
        bar.show_elapsed_time();
        bar.show_remaining_time();
    }

    //first step is to find the total number of basic blocks for allocation
    int blockCount = 0;
    for (auto mi = bitcode->begin(); mi != bitcode->end(); mi++)
    {
        for (auto fi = mi->begin(); fi != mi->end(); fi++)
        {
            blockCount++;
        }
    }

    int openCount[blockCount];            // counter to know where we are in the callstack
    set<int> finalBlocks[kernels.size()]; // final kernel definitions
    set<int> openBlocks;                  // current blocks that are a child
    set<int> kernelMap[blockCount];       // map between a block and the parent kernel
    int kernelStarts[kernels.size()];     // map of a kernel index to the first block seen
    set<int> blocks[kernels.size()];      // temporary kernel blocks
    for (int i = 0; i < blockCount; i++)
    {
        kernelMap[i] = set<int>();
        openCount[i] = 0;
    }
    for (int i = 0; i < kernels.size(); i++)
    {
        blocks[i] = set<int>();
        finalBlocks[i] = set<int>();
        kernelStarts[i] = -1;
    }

    for (int i = 0; i < kernels.size(); i++)
    {
        set<int> kernel = kernels[i];
        for (int block : kernel)
        {
            kernelMap[block].insert(i);
        }
    }

    std::ifstream::pos_type traceSize = filesize(sourceFile);
    int totalBlocks = traceSize / BLOCK_SIZE + 1;
    // File stuff for input trace and output decompressed file
    std::ifstream inputTrace;
    inputTrace.open(sourceFile);
    inputTrace.seekg(0, std::ios_base::end);
    uint64_t size = inputTrace.tellg();
    inputTrace.seekg(0, std::ios_base::beg);
    // Holds blocks of the trace
    char dataArray[BLOCK_SIZE];
    char decompressedArray[BLOCK_SIZE];
    // Zlib decompression object
    z_stream strm;
    int ret;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = inflateInit(&strm);
    assert(ret == Z_OK);
    // Keep track of how many blocks of the trace we've done
    int index = 0;
    // Connector for incomplete lines between blocks
    std::string priorLine = "";
    bool notDone = true;
    // Shows whether we've seen the first block ID yet
    bool seenFirst;
    while (notDone)
    {
        // read a block size of the trace
        inputTrace.readsome(dataArray, BLOCK_SIZE);
        strm.next_in = (Bytef *)dataArray;   // input data to z_lib for decompression
        strm.avail_in = inputTrace.gcount(); // remaining characters in the compressed inputTrace
        std::string bufferString = "";
        while (strm.avail_in != 0)
        {
            std::string strresult;
            // decompress our data
            strm.next_out = (Bytef *)decompressedArray; // pointer where uncompressed data is written to
            strm.avail_out = BLOCK_SIZE;                // remaining space in decompressedArray
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);

            // put decompressed data into a string for splitting
            unsigned int have = BLOCK_SIZE - strm.avail_out;

            for (int i = 0; i < have; i++)
            {
                strresult += decompressedArray[i];
            }
            bufferString += strresult;
            continue;

        } // while(strm.avail_in != 0)

        std::stringstream stringStream(bufferString);
        std::vector<std::string> split;
        std::string segment;

        while (std::getline(stringStream, segment, '\n'))
        {
            split.push_back(segment);
        }

        seenFirst = false;
        int splitIndex = 0;
        for (std::string &it : split)
        {
            auto pi = it;
            if (it == split.front() && !seenFirst)
            {
                it = priorLine + it;
                seenFirst = true;
            }
            else if (splitIndex == split.size() - 1 && bufferString.back() != '\n')
            {
                break;
            }
            // split it by the colon between the instruction and value
            std::stringstream itstream(it);
            std::vector<std::string> spl;
            while (std::getline(itstream, segment, ':'))
            {
                spl.push_back(segment);
            }

            std::string key = spl.front();
            std::string value = spl.back();

            if (key == value)
            {
                break;
            }

            ///////////////////////////////////////////////
            //This is the actual logic
            if (key == "BBEnter")
            {
                int block = stoi(value, 0, 0);
                openCount[block]++; //mark this block as being entered
                openBlocks.insert(block);

                for (int i = 0; i < kernels.size(); i++)
                {
                    blocks[i].insert(block);
                }

                for (auto open : openBlocks)
                {
                    for (auto ki : kernelMap[open])
                    {
                        finalBlocks[ki].insert(block);
                    }
                }

                for (auto ki : kernelMap[block])
                {

                    if (kernelStarts[ki] == -1)
                    {
                        kernelStarts[ki] = block;
                        finalBlocks[ki].insert(block);
                    }
                    if (kernelStarts[ki] != block)
                    {
                        finalBlocks[ki].insert(blocks[ki].begin(), blocks[ki].end());
                    }
                    blocks[ki].clear();
                }
            }
            else if (key == "TraceVersion")
            {
            }
            else if (key == "StoreAddress")
            {
            }
            else if (key == "LoadAddress")
            {
            }
            else if (key == "BBExit")
            {
                int block = stoi(value, 0, 0);
                openCount[block]--;
                if (openCount[block] == 0)
                {
                    openBlocks.erase(block);
                }
            }
            else
            {
                spdlog::critical("Unrecognized key: " + key);
                throw 2;
            }
            splitIndex++;
        }

        if (bufferString.back() != '\n')
        {
            priorLine = split.back();
        }
        else
        {
            priorLine = "";
        }
        index++;

        notDone = (ret != Z_STREAM_END);
        if (index > totalBlocks)
        {
            notDone = false;
        }
        float percent = (float)index / (float)totalBlocks * 100.0f;
        if (!noProgressBar)
        {
            bar.set_progress(percent);
            bar.set_postfix_text("Analyzing block " + to_string(index) + "/" + to_string(totalBlocks));
        }
        else
        {
            int iPercent = (int)percent;
            if (iPercent > previousCount + 5)
            {
                previousCount = ((iPercent / 5) + 1) * 5;
                spdlog::info("Completed block {0:d} of {1:d}", index, totalBlocks);
            }
        }
    }

    if (!noProgressBar && !bar.is_completed())
    {
        bar.mark_as_completed();
    }

    std::set<set<int>> finalSets;
    for (int i = 0; i < kernels.size(); i++)
    {
        finalSets.insert(finalBlocks[i]);
    }
    std::map<int, std::set<int>> finalMap;
    int i = 0;
    for (auto fin : finalSets)
    {
        finalMap[i++] = fin;
    }

    return finalMap;
}
