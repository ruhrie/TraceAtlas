#include "EncodeExtract.h"
#include "EncodeDetect.h"
#include <algorithm>
#include <assert.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <sstream>
#include <zlib.h>

#define BLOCK_SIZE 4096

std::map<int, std::set<int>> ExtractKernels(std::string sourceFile, std::vector<std::set<int>> kernels, bool newline)
{
    /* Data structures for grouping kernels */
    // Dictionary for holding the first blockID for each kernel
    std::map<int, int> kernStart;
    // Dictionary the final set of kernels, [kernelID] -> [blockIDs]
    std::map<int, std::set<int>> finalBlocks;
    // Vector for holding blocks not belonging to a type 1 kernel
    std::map<int, std::set<int>> blocks;

    /* Read the Trace */
    // Compute # of blocks in the trace
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
            // If key is basic block, put it in our sorting dictionary
            if (key == "BasicBlock")
            {
                long int block = stoi(value, 0, 0);
                for (int i = 0; i < kernels.size(); i++)
                {
                    std::set<int> *kernel = &(kernels[i]);
                    if ((kernStart.find(i) != kernStart.end()) && (block == kernStart[i])) //check if i is a key and if we are the start //only second half is needed
                    {
                        blocks[i].clear();
                    }
                    else
                    {
                        blocks[i].insert(block);
                        if (kernel->find(block) != kernel->end()) //block is a part of kernel
                        {
                            if (kernStart.find(i) == kernStart.end()) //we haven't seen the start before ..so assign it
                            {
                                kernStart[i] = block;
                                finalBlocks[i].insert(block);
                            }
                            else
                            {
                                finalBlocks[i].merge(blocks[i]);
                            }
                            blocks[i].clear();
                        }
                    }
                }
            } // if key == BasicBlock
            else if (key == "TraceVersion")
            {
            }
            else if (key == "StoreAddress")
            {
            }
            else if (key == "LoadAddress")
            {
            }
            else
            {
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
        if (index > totalBlocks)
        {
            notDone = false;
        }
        if (index % 1000 == 1)
        {
            std::cout << "Currently reading block " << index << " of " << totalBlocks << ".\n";
        }
    }

    std::vector<std::set<int>> checker;
    for (int i = 0; i < kernels.size(); i++)
    {
        if (std::find(checker.begin(), checker.end(), finalBlocks[i]) == checker.end())
        {
            checker.push_back(finalBlocks[i]);
        }
    }
    std::map<int, std::set<int>> finalMap;
    for (int i = 0; i < checker.size(); i++)
    {
        //std::sort( checker[i].begin(), checker[i].end() );
        //std::vector<int> v(checker[i].begin(), checker[i].end());
        finalMap[i] = checker[i];
    }

    return finalMap;
}
