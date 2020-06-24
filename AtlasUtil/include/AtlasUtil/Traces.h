#pragma once
#include "AtlasUtil/Exceptions.h"
#include <fstream>
#include <functional>
#include <indicators/progress_bar.hpp>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <zlib.h>

#define BLOCK_SIZE 4096

static void ProcessTrace(const std::string &TraceFile, const std::function<void(std::string &, std::string &)> &LogicFunction, const std::string &barPrefix = "", bool noBar = false)
{
    std::cout << "\e[?25l";
    indicators::ProgressBar bar;
    int previousCount = 0;
    if (!noBar)
    {
        bar.set_option(indicators::option::PrefixText{barPrefix});
        bar.set_option(indicators::option::ShowElapsedTime{true});
        bar.set_option(indicators::option::ShowRemainingTime{true});
        bar.set_option(indicators::option::BarWidth{50});
    }

    std::ifstream inputTrace;
    char dataArray[BLOCK_SIZE];
    char decompressedArray[BLOCK_SIZE];
    z_stream strm;
    int ret;

    //init zlib
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = inflateInit(&strm);
    assert(ret == Z_OK);

    int index = 0;

    //open the file
    inputTrace.open(TraceFile);

    if (!inputTrace)
    {
        throw AtlasException("Failed to open trace file");
    }

    //get the file size
    inputTrace.seekg(0, std::ios_base::end);
    int64_t size = inputTrace.tellg();
    inputTrace.seekg(0, std::ios_base::beg);
    int64_t blocks = size / BLOCK_SIZE + 1;

    bool notDone = true;
    bool seenFirst;
    std::string priorLine;

    while (notDone)
    {
        // read a block size of the trace
        inputTrace.read(dataArray, BLOCK_SIZE);
        int64_t bytesRead = inputTrace.gcount();
        strm.next_in = (Bytef *)dataArray;   // input data to z_lib for decompression
        strm.avail_in = (uint32_t)bytesRead; // remaining characters in the compressed inputTrace
        while (strm.avail_in != 0)
        {
            // decompress our data
            strm.next_out = (Bytef *)decompressedArray; // pointer where uncompressed data is written to
            strm.avail_out = BLOCK_SIZE - 1;            // remaining space in decompressedArray
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);

            // put decompressed data into a string for splitting
            unsigned int have = BLOCK_SIZE - strm.avail_out;
            decompressedArray[have - 1] = '\0';
            std::string bufferString(decompressedArray);
            std::stringstream stringStream(bufferString);
            std::string segment;
            std::getline(stringStream, segment, '\n');
            char back = bufferString.back();
            seenFirst = false;
            while (true)
            {
                if (!seenFirst)
                {
                    segment = priorLine.append(segment);
                    seenFirst = true;
                }
                // split it by the colon between the instruction and value
                std::stringstream itstream(segment);
                std::string key;
                std::string value;
                std::string error;
                std::getline(itstream, key, ':');
                std::getline(itstream, value, ':');
                bool fin = false;
                if (!std::getline(stringStream, segment, '\n'))
                {
                    if (back == '\n')
                    {
                        fin = true;
                    }
                    else
                    {
                        break;
                    }
                }

                //process the line here
                LogicFunction(key, value);
                if (fin)
                {
                    break;
                }
            }
            priorLine = segment;
        }
        index++;
        notDone = (ret != Z_STREAM_END);
        if (index > blocks)
        {
            notDone = false;
        }
        float percent = (float)index / (float)blocks * 100.0f;
        if (!noBar)
        {
            bar.set_progress(percent);
            bar.set_option(indicators::option::PostfixText{"Block " + std::to_string(index) + "/" + std::to_string(blocks)});
        }
        else
        {
            int iPercent = (int)percent;
            if (iPercent > previousCount + 5)
            {
                previousCount = ((iPercent / 5) + 1) * 5;
                //spdlog::info("Completed block {0:d} of {1:d}", index, blocks);
            }
        }
    }

    if (!noBar && !bar.is_completed())
    {
        bar.mark_as_completed();
    }

    inflateEnd(&strm);
    inputTrace.close();
    std::cout << "\e[?25h";
}