
#include "EncodeDetect.h"
#include <algorithm>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <zlib.h>

#define BLOCK_SIZE 4096

std::ifstream::pos_type filesize(const char *filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

void DetectKernels(char *sourceFile, float thresh, int hotThresh, bool newline)
{
    //std::cout << "Detecting type one Kernels.\n";
    std::vector<int> result;
    int radius = 5;

    std::map<int, std::map<int, int>> blockMap;
    std::map<int, int> blockCount;
    std::ifstream::pos_type traceSize = filesize(sourceFile);
    int blocks = traceSize / BLOCK_SIZE + 1;
    //std::cout << "Blocks in this trace is " << blocks << "\n";

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
    //std::cout << "Starting trace parse.\n";
	int c = 0;
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

		// split bufferString by \n's
		std::stringstream stringStream(bufferString);
		std::vector<std::string> split;
		std::string segment;

		while (std::getline(stringStream, segment, '\n'))
		{
			split.push_back(segment);
			//std::cout << split.back() << "\n";
		}
		//std::cout << "Now we've split the vector.\n";

		// Now parse the line into a dictionary with block as key and count as value
		//std::cout << "printing split ...\n";

		for (std::string it : split)
		{
			//std::cout << it << "\n";
			if (it == split.back() && bufferString.back() != '\n')
			{
				break;
			}
			auto pi = it;
			if (it == split.front())
			{
				it = priorLine + it;
			}
			//std::cout << "Started parsing a line.\n";
			// split it by the colon between the instruction and value
			std::stringstream itstream(it);
			std::vector<std::string> spl;
			while (std::getline(itstream, segment, ':'))
			{
				spl.push_back(segment);
			}
			//std::cout << "Just split the line by its colon.\n";
			std::string key = spl.front();
			std::string value = spl.back();
			/*if (spl.front() == "BasicBlock" && spl.back() == "0X4")
			{
				std::cout << "BB 4 was found in block " << index << " and its count is " << blockCount[block] << std::endl;
			}*/
			//std::cout << it << "\n";
			//std::cout << key << "\n";
			//std::cout << value << "\n";
			if (key == value)
			{
				break;
			}
			// If key is basic block, put it in our sorting dictionary
			if (key == "BasicBlock")
			{
				//std::cout << "This line is a basic block.\n";
				long int block = stoi(value, 0, 0);
				if (block == 0)
				{
					//std::cout << block << " : " << value << "\n";
				}
				blockCount[block] += 1;
				priorBlocks.push_back(block);
				//std::cout << "Just put the last block in priorBlocks.\n";
				if (priorBlocks.size() > (2 * radius + 1))
				{
					priorBlocks.erase(priorBlocks.begin());
				}
				if (spl.front() == "BasicBlock" && spl.back() == "0X4")
				{
					std::cout << "BB 4 was found in block " << index << " and its count is " << blockCount[block] << std::endl;
				}
				if (priorBlocks.size() > radius)
				{
					//std::cout << "Iterating block in blockMap.\n";
					for( auto i : priorBlocks )
					{
						//std::cout << block << "," << i << "\n";
						blockMap[block][i]++;
					}
					//std::cout << "Done iterating block.\n";
				} // if priorBlocks.size > radius
					//std::cout << "Just parsed the basic block.\n";
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
		outfile << bufferString;
        if (index > blocks)
        {
            notDone = false;
        }
        if (index % 100 == 0)
        {
            //std::cout << "Currently reading block " << index << " of " << blocks << ".\n";
        }
    } // while( notDone)

    //std::cout << "notDone is updated.\n";
    //std::cout << "index is " << index << ".\n";
    for (auto elem : blockCount)
    {
        //std::cout << elem.first << " : " << elem.second << "\n";
    }

    // assign to every index of every list value in blockMap a normalized amount
    std::map<int, std::map<int, float>> fBlockMap;
    std::vector<std::set<int>> kernels;
    for (auto &key : blockMap)
    {
        int total = 0;
        for (auto &sub : key.second)
        {
            total += sub.second;
        }
        for (auto &sub : key.second)
        {
            fBlockMap[key.first][sub.first] = (float)sub.second / (float)total;
        }
    }
    /*for (auto &key : blockMap)
    {
        std::cout << key.first << ": ";
        for (auto &sub : key.second)
        {
            std::cout << sub.second << ", ";
        }
        std::cout << "\n";
    }
    for (auto &key : fBlockMap)
    {
        std::cout << key.first << ": ";
        for (auto &sub : key.second)
        {
            std::cout << sub.second << ", ";
        }
        std::cout << "\n";
    }*/

    /*
    std::set<int> covered;
    std::map <int, int> sBlockCount;
    for( auto it = blockCount.begin(); it != blockCount.end(); it++ )
    {
	    if( (*it).second.first > hotThresh )
	    {
		    if( covered.find( (*it).first ) )
		    {
			    float sum = 0.0;
			    std::vector<int> values = blockMap[block];
			    sort( values.begin(), values.end() );
			    std::set kernel;
			    while( sum < thresh )
			    {
				    int entry = values.front();
				    covered.insert( entry );
				    values.remove( values.front() );
				    sum += entry.find(1);
				    kernel.insert( entry.front() );
			    }
			    kernels.insert( kernel );
		    } // if covered
	    } // if > hotThresh
	    else
	    {
		    break;
	    }
    } // for it in blockCount

    for( auto it = kernels.begin(); it != kernels.end(); it++ )
    {
	    if( !result.find(it) )
	    {
		    result.insert(it);
	    }
    }
*/
}
