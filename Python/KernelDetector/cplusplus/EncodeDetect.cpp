
#include <algorithm>
#include <iostream>
#include <list>
#include <set>
#include <vector>
#include <string>
#include <sstream>
#include <map>
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

void DetectKernels(char* sourceFile, float thresh, int hotThresh, bool newline)
{
	std::cout << "Detecting type one Kernels.\n";
	std::vector<int> result;
	int radius = 5;

	std::map <int, std::map<int, int> > blockMap;
	std::map <int, int> blockCount;
	std::ifstream::pos_type traceSize = filesize(sourceFile);
	int blocks = traceSize / BLOCK_SIZE + 1;
	std::cout << "Blocks in this trace is " << blocks << "\n";

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
    inputTrace.open(sourceFile);
    inputTrace.seekg(0, std::ios_base::end);
    uint64_t size = inputTrace.tellg();
    inputTrace.seekg(0, std::ios_base::beg);

    // dictionary for kernel sorting
    std::map< int, std::set<int> > kernelMap;

	// some variables for the loop
	int index = 0;
	std::string priorLine = "";
	//bar = cs(newLine);
	//bar.SetMax(blocks);
	//bar.Start();

	// vectors to read the file into line by line
    bool notDone = true;
    int lastBlock;
    std::string strresult;
    std::vector<std::string> fileList;
    std::vector<int> priorBlocks;
    //std::vector<std::string>::iterator it = fileList.begin();
    std::cout << "Starting trace parse.\n";
    while (notDone)
    {
	index++;
	// read a block size of the trace
        inputTrace.readsome(dataArray, BLOCK_SIZE);
        strm.next_in = (Bytef *)dataArray; // input data to z_lib for decompression
        strm.avail_in = inputTrace.gcount(); // remaining characters in the compressed inputTrace
        while (strm.avail_in != 0)
        {
    	    // decompress our data
            strm.next_out = (Bytef *)decompressedArray; // pointer where uncompressed data is written to
            strm.avail_out = BLOCK_SIZE; // remaining space in decompressedArray
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);

	    // put decompressed data into a string for splitting 
	    unsigned int have = BLOCK_SIZE-strm.avail_out;
	    std::cout << "have is " << have << ".\n";
	    for( int i = 0; i < have; i++ )
	    {
		    strresult += decompressedArray[i];
	    }
	    //std::cout << strresult << "\n";
    	    //std::cout << "Decompressed the data and put it into a vector of BLOCK_SIZE.\n";

	    // split strresult by \n's
	    std::stringstream stringStream(strresult);
	    std::vector<std::string> split;
	    std::string segment;
	    while(std::getline( stringStream, segment, '\n' ))
	    {
		    split.push_back(segment);
	    }
	    //std::cout << "Now we've split the vector.\n";

	    // Now parse the line into a dictionary with block as key and count as value
	    for( std::string it : split )
	    {
		    if( it == split.front() )
		    {
			    it += priorLine ;
		    }
	    	    //std::cout << "Started parsing a line.\n";
		    // split it by the colon between the instruction and value
		    std::stringstream itstream(it);
		    std::vector<std::string> spl;
		    while( std::getline(itstream, segment, ':') )
		    {
			    spl.push_back(segment);
		    }
	    	    //std::cout << "Just split the line by its colon.\n";
		    std::string key = spl.front();
		    std::string value = spl.back();
		    //std::cout << it << "\n";
		    //std::cout << key << "\n";
		    //std::cout << value << "\n";
		    if( key == value )
		    {
			    break;
		    }
		    
		    // If key is basic block, put it in our sorting dictionary
		    if( key == "BasicBlock" )
		    {
	    	    	    //std::cout << "This line is a basic block.\n";
			    int block = stoi(value, 0, 0);
			    blockCount[block] += 1;
			    priorBlocks.push_back(block);
	    	    	    ///std::cout << "Just put the last block in priorBlocks.\n";
			    if( priorBlocks.size() > (2 * radius + 1) )
			    {
				    priorBlocks.erase( priorBlocks.begin() );
			    }
			    if( priorBlocks.size() > radius )
			    {
				    //std::cout << "Iterating block in blockMap.\n";
				    for( int i = 0; i < priorBlocks.size(); i++ )
				    {
					    //std::cout << block << "," << i << "\n";
					    blockMap[block][i]++;
				    }
				    //std::cout << "Done iterating block.\n";
			    } // if priorBlocks.size > radius
	    	    	    //std::cout << "Just parsed the basic block.\n";
		    } // if key == BasicBlock
	    } // for it in split
	    priorLine = split.back();
	    // status bar stuff 
	    // if( index % 100 == 0 )
	    // {
	    //     update bar
	    // }
	    // if( have < BLOCK_SIZE )
	    // {
	    //     we're done
	    // }
	} // while(strm.avail_in != 0)
	notDone = ( ret != Z_STREAM_END );
	if( index > blocks )
	{
		notDone = false;
	}
	//std::cout << "notDone is updated.\n";
    	for( auto elem : blockCount)
    	{
		std::cout << elem.first << "\n";
		std::cout << elem.second<< "\n";
		/*
		for( auto key : blockMap[elem.first] )
		{
	        	std::cout << key.first << "\n";//<< key.second.first << "," << key.second.second << "\n";
		}
		*/
    	}
    } // while( notDone)

}








/*
	    int l = 0;
	    for( string line : split )
	    {
		    l++;
		    if( line == split.back() && result.back() != '\n' )
		    {
			    result = line;
		    }
		    else
		    {
			    std::stringstream lineStream(line);
			    string key, value;
			    getline(lineStream, key, ':');
			    getline(lineStream, value, ':');
			    if( key == "BasicBlock" )
			    {
				    int block = stoi(value, 0, 0);
				    blockCount[block]+=1;
				    lastBlock = block;
				    if( currentKernel == "-1" || kernelMap[currentKernel].find(block) == kernelMap[currentKernel].end() )
				    {
					    // we aren't in the same kernel as last iteration
					    string innerKernel = "-1";
					    for( auto k : kernelMap )
					    {
						    if( k.second.find(block) != k.second.end() )
						    {
							    // we have a matching kernel
							    innerKernel = k.first;
							    break;
						    }
					    }
					    currentKernel = innerKernel;
					    if( innerKernel != "-1" )
					    {
						    currentUid = UID;
						    kernelIdMap[UID++] = currentKernel;
					    }
				    }
				    basicBlocks.push_back(block);
			    }
			    else if( key == "LoadAddress" );
			    {
				    uint64_t address = stoul( value, 0, 0 );
				    int prodUid = writeMap[address];
				    

	}
		//notDone = (ret != Z_STREAM_END);
		notDone = true;
    }
    */
	/* input verification
    for( std::vector<std::string>::iterator it = fileList.begin(); it != fileList.end(); it++ )
    {
	for( int i = 0; i < BLOCK_SIZE; i++ )
	{
		std::cout << it[i];
	}
    }
		
		
}

    */

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

	
