#pragma once
#include <atomic>
#include <fstream>
#include <functional>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <thread>
#include <zlib.h>

#define BLOCK_SIZE 4096

static void ProcessTrace(const std::string &TraceFile, const std::function<void(std::vector<std::string> &)> &LogicFunction, std::atomic<int> *completeCounter = nullptr)
{
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
        inputTrace.readsome(dataArray, BLOCK_SIZE);
        strm.next_in = (Bytef *)dataArray;             // input data to z_lib for decompression
        strm.avail_in = (uint32_t)inputTrace.gcount(); // remaining characters in the compressed inputTrace
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
                std::vector<std::string> lineValues;
                std::string key;
                std::string value;
                std::string error;
                std::string val;
                while (itstream.good())
                {
                    getline(itstream, val, ':');
                    lineValues.push_back(val);
                }
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
                LogicFunction(lineValues);
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
    }

    inflateEnd(&strm);
    inputTrace.close();
    if (completeCounter)
    {
        (*completeCounter) += 1;
    }
}

static void ProcessTraces(const std::vector<std::string> &TraceFiles, const std::function<void(std::vector<std::string> &)> &LogicFunction, std::function<void()> *setup = nullptr, std::function<void()> *reset = nullptr)
{
    std::vector<std::thread> threads;
    for (const std::string &trace : TraceFiles)
    {
        std::thread t([&] {
            if (setup != nullptr)
            {
                (*setup)();
            }
            ProcessTrace(trace, LogicFunction, nullptr);
            if (reset != nullptr)
            {
                (*reset)();
            }
        });
        threads.push_back(move(t));
    }
    for (auto &t : threads)
    {
        t.join();
    }
}