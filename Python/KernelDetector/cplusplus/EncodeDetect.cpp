
#include <algorithm>
#include <list>
#include <set>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <functional>
#include <assert.h>
#include <zlib.h>
#include "EncodeDetect.h"

#define 	BLOCK_SIZE	4096

std::ifstream::pos_type filesize(const char* filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

std::vector< std::set< int > > DetectKernels(char *sourceFile, float thresh, int hotThresh, bool newline)
{
	/* Structures for grouping kernels */
	// Tracer parameter, sets the maximum width that the parser can look for temporally affine blocks
    int radius = 5;
	// Maps a blockID to its vector of temporally affine blocks
    std::map<int, std::map<int, int>> blockMap;
	// Holds the count of each blockID
    std::map<int, int> blockCount;
    // Maps a kernelID to its set of blockIDs
    std::map<int, std::set<int>> kernelMap;
	// Vector for grouping blocks together 
    std::vector<int> priorBlocks;

    /* Read the Trace */
	// Compute # of blocks in the trace
    std::ifstream::pos_type traceSize = filesize(sourceFile);
    int blocks = traceSize / BLOCK_SIZE + 1;
	// File stuff for input trace and output decompressed file
    std::ifstream inputTrace;
	std::ofstream outfile;
	outfile.open("outfile.txt");
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
        if (index % 100 == 0)
        {
            std::cout << "Currently reading block " << index << " of " << blocks << ".\n";
        }
    } // while( notDone )

	/*
	std::cout << "About to output counts.\n";
    for (auto elem : blockCount)
    {
        std::cout << elem.first << " : " << elem.second << "\n";
    }*/

    // assign to every index of every list value in blockMap a normalized amount
    std::map<int, std::vector< std::pair<int, float> > > fBlockMap;
    for (auto &key : blockMap)
    {
        int total = 0;
        for (auto &sub : key.second)
        {
            total += sub.second;
        }
        for (auto &sub : key.second)
        {
            fBlockMap[key.first].push_back( std::pair<int, float>( sub.first, (float)sub.second / (float)total ) );
        	//fBlockMap[key.first][sub.first] = (float)sub.second / (float)total;
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

    std::set<int> covered;
	std::vector< std::set< int > > kernels;

	// sort blockMap in descending order of values by making a vector of pairs
	std::vector< std::pair<int, int> > blockPairs;
	for( auto iter = blockCount.begin(); iter != blockCount.end(); iter++ )
	{
		blockPairs.push_back( *iter );
	}

	std::sort( blockPairs.begin(), blockPairs.end(), [=](std::pair<int, int>& a, std::pair<int, int>& b)
	{
		if( a.second > b.second )
		{
			return true;
		}
		else if( a.second == b.second)
		{
			if( a.first < b.first )
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
	} );

    for( auto &it : blockPairs )
    {
	    if( it.second > hotThresh )
	    {
		    if( covered.find( it.first ) == covered.end() )
		    {
				std::cout << "using seed " << it.first << "\n";
			    float sum = 0.0;
			    //std::vector<float> values = fBlockMap.find( &it.first );
			    auto values = fBlockMap.find( it.first );		
				std::sort( values->second.begin(), values->second.end(), [=](std::pair<int, float>& a, std::pair<int, float>& b)
				{
					if( a.second > b.second )
					{
						return true;
					}
					else if( a.second == b.second)
					{
						if( a.first < b.first )
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
				} );
			    std::set< int >kernel;
			    while( sum < thresh )
			    {
				    std::pair< int, float > entry = values->second[0];
				    covered.insert( entry.first );
				    std::remove( values->second.begin(), values->second.end(), entry );
				    sum += entry.second;
				    kernel.insert( entry.first );
			    }
			    kernels.push_back( kernel );
		    } // if covered
	    } // if > hotThresh
	    else
	    {
		    break;
	    }
    } // for it in blockCount

	std::vector< std::set< int > > result;
    for( auto it : kernels )
    {
	    if( std::find( result.begin(), result.end(), it) == result.end() )
	    {
		    result.push_back(it);
	    }
    }
	return result;
}
