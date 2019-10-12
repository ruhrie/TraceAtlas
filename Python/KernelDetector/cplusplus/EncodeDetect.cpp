
#include <iostream>
#include <list>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <assert.h>
#include <zlib.h>
#include "EncodeDetect.hpp"

#define 	BLOCK_SIZE	4096

std::ifstream::pos_type filesize(const char* filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

void DetectKernels(char* sourceFile, float thresh, int hotThresh, bool newline)
{
	std::cout << "Detecting type one Kernels.\n";
	std::vector<int> result;
	int radius = 5;

	std::map <std::string, float> blockMap;
	std::map <std::string, float> blockCount;
	std::ifstream::pos_type traceSize = filesize(sourceFile);
	int blocks = traceSize / BLOCK_SIZE + 1;

	// some variables for the loop
	int index = 0;
	std::string priorLine = "";
	//bar = cs(newLine);
	//bar.SetMax(blocks);
	//bar.Start();

	// now read the trace
    std::ifstream inputTrace;
    char dataArray[BLOCK_SIZE];
    char decompressedArray[BLOCK_SIZE];

    z_stream strm;
    int ret;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = inflateInit(&strm);
    assert(ret == Z_OK);
    inputTrace.open(sourceFile);

    inputTrace.seekg(0, std::ios_base::end);
    uint64_t size = inputTrace.tellg();
    inputTrace.seekg(0, std::ios_base::beg);

    bool notDone = true;
    int lastBlock;
	std::vector<char[BLOCK_SIZE]> fileList;
	std::vector<char[BLOCK_SIZE]>::iterator it = fileList.begin();
    while (notDone)
    {
        inputTrace.readsome(dataArray, BLOCK_SIZE);
        strm.next_in = (Bytef *)dataArray;
        strm.avail_in = inputTrace.gcount();
		// read our trace into a list
        while (strm.avail_in != 0)
        {
            strm.next_out = (Bytef *)decompressedArray;
            strm.avail_out = BLOCK_SIZE;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);
			std::cout << decompressedArray;
			

			it = fileList.insert(it, decompressedArray);

		}
		notDone = (ret != Z_STREAM_END);
	}

	for( std::vector<char[BLOCK_SIZE]>::iterator it = fileList.begin(); it != fileList.end(); it++ )
	{
		for( int i = 0; i < BLOCK_SIZE; i++ )
		{
			//std::cout << it[i];
		}
	}
		
		
}


/*
	// loop until we have covered every basic block
	while( true )
	{
		std::vector <std::string> lines;
		while( std::getline(stream,line) )
		{
			lines.push_back(line);
		}

		// split line by ;
		std::vector<std::string>::iterator line;
		for( line = lines.begin(); line != lines.end(); line++)
		{
			if( line == lines.begin() )
			{
				line = priorLine + line;
			}
		}
	}
		
	*/	

	
