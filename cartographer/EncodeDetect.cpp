#include "EncodeDetect.h"
#include "cartographer.h"
#include <algorithm>
#include <assert.h>
#include <deque>
#include <functional>
#include <indicators/progress_bar.hpp>
#include <iostream>
#include <list>
#include <map>
#include <spdlog/spdlog.h>
#include <sstream>
#include <zlib.h>

#define BLOCK_SIZE 4096

using namespace std;

std::map<int, int> blockCount; //fine

std::ifstream::pos_type filesize(std::string filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

std::vector<std::set<int>> DetectKernels(std::string sourceFile, float thresh, int hotThresh, bool newline)
{
    indicators::ProgressBar bar;
    int previousCount = 0;
    if (!noProgressBar)
    {
        bar.set_prefix_text("Detecting type 1 kernels");
        bar.show_elapsed_time();
        bar.show_remaining_time();
    }

    /* Structures for grouping kernels */
    // Tracer parameter, sets the maximum width that the parser can look for temporally affine blocks
    int radius = 5;
    // Maps a blockID to its vector of temporally affine blocks
    std::map<int, std::map<int, int>> blockMap; //should probably seperate the floating point values into a seperate structure
    // Holds the count of each blockID

    // Vector for grouping blocks together
    std::deque<int> priorBlocks; //should be a dequeue

    /* Read the Trace */
    // Compute # of blocks in the trace
    std::ifstream::pos_type traceSize = filesize(sourceFile);
    int blocks = traceSize / BLOCK_SIZE + 1;
    //bar.set_bar_width(blocks);
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
    bool seenFirst, seenLast;
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
            if (it == split.front() && !seenFirst)
            {
                it = priorLine + it;
                seenFirst = true;
            }
            else if (splitIndex == split.size() - 1 && bufferString.back() != '\n')
            {
                seenLast = true;
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
            // If key is basic block, put it in our sorting dictionary
            if (key == "BBEnter")
            {
                long int block = stoi(value, 0, 0);
                blockCount[block] += 1;
                priorBlocks.push_back(block);

                if (priorBlocks.size() > (2 * radius + 1))
                {
                    priorBlocks.pop_front();
                }
                if (priorBlocks.size() > radius)
                {
                    for (auto i : priorBlocks)
                    {
                        blockMap[block][i]++;
                    }
                } // if priorBlocks.size > radius
            }     // if key == BasicBlock
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
            }
            else
            {
                spdlog::critical("Unrecognized key: " + key);
                throw 2;
            }
            splitIndex++;
        } // for it in split
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
        if (index > blocks)
        {
            notDone = false;
        }
        float percent = (float)index / (float)blocks * 100.0f;
        if (!noProgressBar)
        {
            bar.set_progress(percent);
            bar.set_postfix_text("Analyzing block " + to_string(index) + "/" + to_string(blocks));
        }
        else
        {
            int iPercent = (int)percent;
            if (iPercent > previousCount + 5)
            {
                previousCount = ((iPercent / 5) + 1) * 5;
                spdlog::info("Completed block {0:d} of {1:d}", index, blocks);
            }
        }
    } // while( notDone )

    if (!noProgressBar && !bar.is_completed())
    {
        bar.mark_as_completed();
    }

    // assign to every index of every list value in blockMap a normalized amount
    std::map<int, std::vector<std::pair<int, float>>> fBlockMap; //really this is a matrix of floats
    for (auto &key : blockMap)
    {
        int total = 0;
        for (auto &sub : key.second)
        {
            total += sub.second;
        }
        for (auto &sub : key.second)
        {
            fBlockMap[key.first].push_back(std::pair<int, float>(sub.first, (float)sub.second / (float)total));
        }
    }

    std::set<int> covered;
    std::vector<std::set<int>> kernels;

    // sort blockMap in descending order of values by making a vector of pairs
    std::vector<std::pair<int, int>> blockPairs;
    for (auto iter = blockCount.begin(); iter != blockCount.end(); iter++)
    {
        blockPairs.push_back(*iter);
    }

    std::sort(blockPairs.begin(), blockPairs.end(), [=](std::pair<int, int> &a, std::pair<int, int> &b) {
        if (a.second > b.second)
        {
            return true;
        }
        else if (a.second == b.second)
        {
            if (a.first < b.first)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    });
    for (auto &it : blockPairs)
    {
        if (it.second > hotThresh)
        {
            if (covered.find(it.first) == covered.end())
            {
                float sum = 0.0;
                vector<pair<int, float>> values = fBlockMap[it.first];
                std::sort(values.begin(), values.end(), [=](std::pair<int, float> &a, std::pair<int, float> &b) {
                    if (a.second > b.second)
                    {
                        return true;
                    }
                    else if (a.second == b.second)
                    {
                        if (a.first < b.first)
                        {
                            return true;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        return false;
                    }
                });
                std::set<int> kernel;
                while (sum < thresh)
                {
                    std::pair<int, float> entry = values[0];
                    covered.insert(entry.first);
                    std::remove(values.begin(), values.end(), entry);
                    sum += entry.second;
                    kernel.insert(entry.first);
                }
                kernels.push_back(kernel);
            } // if covered
        }     // if > hotThresh
        else
        {
            break;
        }
    } // for it in blockCount

    std::vector<std::set<int>> result;
    for (auto &it : kernels)
    {
        if (std::find(result.begin(), result.end(), it) == result.end())
        {
            result.push_back(it);
        }
    }
    return result;
}
