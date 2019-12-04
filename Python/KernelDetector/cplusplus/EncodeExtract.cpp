
#include <algorithm>
#include <iostream>
#include <list>
#include <set>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <functional>
#include <fstream>
#include <assert.h>
#include <zlib.h>
#include "EncodeDetect.h"

#define 	BLOCK_SIZE	4096

std::ifstream::pos_type filesize(const char* filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

std::vector< std::set< int > > ExtractKernels(char *sourceFile, float thresh, int hotThresh, bool newline)
{
    int radius = 5;
    std::map<int, std::map<int, int>> blockMap;
    std::map<int, int> blockCount;
    std::ifstream::pos_type traceSize = filesize(sourceFile);
    int blocks = traceSize / BLOCK_SIZE + 1;

    // now read the trace
    std::ifstream inputTrace;
    char dataArray[BLOCK_SIZE];
    char decompressedArray[BLOCK_SIZE];

    // decompression object
    z_stream strm;
    int ret;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = inflateInit(&strm);
    assert(ret == Z_OK);

    // create file object
	std::ofstream outfile;
	outfile.open("outfile.txt");
    inputTrace.open(sourceFile);
    inputTrace.seekg(0, std::ios_base::end);
    uint64_t size = inputTrace.tellg();
    inputTrace.seekg(0, std::ios_base::beg);

    // dictionary for kernel sorting
    std::map<int, std::set<int>> kernelMap;

    // some variables for the loop
    int index = 0;
    std::string priorLine = "";

    // vectors to read the file into line by line
    bool notDone = true;
    int lastBlock;

    std::vector<std::string> fileList;
    std::vector<int> priorBlocks;
	int c = 0;
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
		for (std::string it : split)
		{
			auto pi = it;
			if (it == split.front() && !seenFirst)
			{
				it = priorLine + it;
				seenFirst = true;
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
				blockCount[block] += 1;
				priorBlocks.push_back(block);

				if (priorBlocks.size() > (2 * radius + 1))
				{
					priorBlocks.erase(priorBlocks.begin());
				}
				if (priorBlocks.size() > radius)
				{
					for( auto i : priorBlocks )
					{
						blockMap[block][i]++;
					}
				} // if priorBlocks.size > radius
			}// if key == BasicBlock
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
        if (index % 1000 == 0)
        {
            std::cout << "Currently reading block " << index << " of " << blocks << ".\n";
        }
	}
}

