#include "Backend/BackendTrace.h"
#include "Backend/Fifo.h"
#include <assert.h>
#include <atomic>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <zlib.h>

using namespace std;

queue<string> writeQueue;

pthread_t tracingThread;
FILE *myfile;
pthread_mutex_t tracingMutex;
pthread_mutexattr_t tracingAttr;
std::hash<std::thread::id> threadHasher;
//trace functions
z_stream strm_DashTracer;
std::atomic<bool> stopTracing = false;
atomic<bool> initialized = false;

int TraceCompressionLevel;
char *TraceFilename;
#define BUFSIZE 128 * 1024
unsigned int bufferIndex = 0;
uint8_t temp_buffer[BUFSIZE];
uint8_t storeBuffer[BUFSIZE];

void WriteStream(char *input)
{
    /*
    size_t size = strlen(input);
    pthread_mutex_lock(&tracingMutex);
    if (bufferIndex + size >= BUFSIZE)
    {
        BufferData();
    }
    memcpy(storeBuffer + bufferIndex, input, size);
    bufferIndex += size;
    pthread_mutex_unlock(&tracingMutex);
    */
    //if (input != NULL)

    pthread_mutex_lock(&tracingMutex);
    //printf("%s", input);
    taFifoPush(input);
    pthread_mutex_unlock(&tracingMutex);
}

void *ProcessTrace(void *arg)
{
    char *tcl = getenv("TRACE_COMPRESSION");
    if (tcl != NULL)
    {
        int l = atoi(tcl);
        TraceCompressionLevel = l;
    }
    else
    {
        TraceCompressionLevel = 5;
    }
    strm_DashTracer.zalloc = Z_NULL;
    strm_DashTracer.zfree = Z_NULL;
    strm_DashTracer.opaque = Z_NULL;
    int ret = deflateInit(&strm_DashTracer, TraceCompressionLevel);
    assert(ret == Z_OK);

    initialized = true;
    while (true)
    {
        while (taFifoEmpty())
        {
            if (stopTracing)
            {
                printf("here");
                strm_DashTracer.next_in = storeBuffer;
                strm_DashTracer.avail_in = bufferIndex;
                strm_DashTracer.next_out = temp_buffer;
                strm_DashTracer.avail_out = BUFSIZE;
                int deflate_res = deflate(&strm_DashTracer, Z_FINISH);
                assert(deflate_res == Z_STREAM_END);

                deflateEnd(&strm_DashTracer);
                for (int i = 0; i < BUFSIZE - strm_DashTracer.avail_out; i++)
                {
                    fputc(temp_buffer[i], myfile);
                }

                return NULL;
            }
        }
        while (true)
        {
            if (taFifoEmpty())
            {
                printf("exit");
                break;
            }
            else
            {
                printf("enter");
                char* input = taFifoPop();
                printf("%s", input);
                //printf("hi %s%lu\n", input.c_str(), writeQueue.size());
            }
        }

        /*
            size_t size = strlen(input);
            if (bufferIndex + size >= BUFSIZE)
            {
                BufferData();
            }
            memcpy(storeBuffer + bufferIndex, input, size);
            bufferIndex += size;
            */
        //writeQueue.pop();
    }
}

///Modified from https://stackoverflow.com/questions/4538586/how-to-compress-a-buffer-with-zlib
void BufferData()
{
    strm_DashTracer.next_in = storeBuffer;
    strm_DashTracer.avail_in = bufferIndex;
    strm_DashTracer.next_out = temp_buffer;
    strm_DashTracer.avail_out = BUFSIZE;
    while (strm_DashTracer.avail_in != 0)
    {
        int res = deflate(&strm_DashTracer, Z_NO_FLUSH);
        assert(res == Z_OK);

        if (strm_DashTracer.avail_out == 0)
        {
            for (int i = 0; i < BUFSIZE; i++)
            {
                fputc(temp_buffer[i], myfile);
            }
            strm_DashTracer.next_out = temp_buffer;
            strm_DashTracer.avail_out = BUFSIZE;
        }
    }
    for (int i = 0; i < BUFSIZE - strm_DashTracer.avail_out; i++)
    {
        fputc(temp_buffer[i], myfile);
    }
    strm_DashTracer.next_out = temp_buffer;
    strm_DashTracer.avail_out = BUFSIZE;
    bufferIndex = 0;
}

void Write(char *inst, int line, int block, uint64_t func)
{
    char suffix[128];
#if defined _WIN32
    sprintf(suffix, ";line:%d;block:%d;function:%llu\n", line, block, func);
#else
    sprintf(suffix, ";line:%d;block:%d;function:%lu\n", line, block, func);
#endif
    size_t size = strlen(inst) + strlen(suffix);
    char fin[size];
    strcpy(fin, inst);
    strncat(fin, suffix, 128);
    WriteStream(fin);
}

void WriteAddress(char *inst, int line, int block, uint64_t func, char *address)
{
    char suffix[128];
#if defined _WIN32
    sprintf(suffix, ";line:%d;block:%d;function:%llu;address:%llu\n", line, block, func, (uint64_t)address);
#else
    sprintf(suffix, ";line:%d;block:%d;function:%lu;address:%lu\n", line, block, func, (uint64_t)address);
#endif
    size_t size = strlen(inst) + strlen(suffix);
    char fin[size];

    strcpy(fin, inst);
    strncat(fin, suffix, 128);
    WriteStream(fin);
}

extern "C" void OpenFile()
{
    char *tfn = getenv("TRACE_NAME");
    if (tfn != NULL)
    {
        TraceFilename = tfn;
    }
    else
    {
        TraceFilename = const_cast<char *>("raw.trc");
    }

    pthread_create(&tracingThread, NULL, &ProcessTrace, NULL);
    while (!initialized)
        ;
    myfile = fopen(TraceFilename, "w");
    WriteStream(const_cast<char *>("TraceVersion:4\n"));
}

extern "C" void CloseFile()
{
    stopTracing = true;
    pthread_join(tracingThread, NULL);
    //fclose(myfile); //breaks occasionally for some reason. Likely a glibc error.
}

extern "C" void LoadDump(void *address)
{
    char fin[128];
    uint64_t thId = threadHasher(std::this_thread::get_id());
    sprintf(fin, "LoadAddress:%#lX:%#lX\n", (uint64_t)address, thId);
    WriteStream(fin);
}
extern "C" void DumpLoadAddrValue(void *MemValue, int size)
{
    char fin[128];
    uint64_t thId = threadHasher(std::this_thread::get_id());
    sprintf(fin, "LoadAddress:%#lX\n", (uint64_t)MemValue);
    WriteStream(fin);
    uint8_t *bitwisePrint = (uint8_t *)MemValue;
    sprintf(fin, "size:%d, LoadMemValue:", size);
    WriteStream(fin);
    for (int i = 0; i < size; i++)
    {
        sprintf(fin, "%u ", bitwisePrint[i]);
        WriteStream(fin);
    }
    sprintf(fin, "\n");
    WriteStream(fin);
}
extern "C" void StoreDump(void *address)
{
    char fin[128];
    uint64_t thId = threadHasher(std::this_thread::get_id());
    sprintf(fin, "StoreAddress:%#lX:%#lX\n", (uint64_t)address, thId);
    WriteStream(fin);
}

extern "C" void DumpStoreAddrValue(void *MemValue, int size)
{
    char fin[128];
    sprintf(fin, "StoreAddress:%#lX\n", (uint64_t)MemValue);
    WriteStream(fin);
    uint8_t *bitwisePrint = (uint8_t *)MemValue;
    sprintf(fin, "size:%d, StoreMemValue:", size);
    WriteStream(fin);
    for (int i = 0; i < size; i++)
    {
        sprintf(fin, "%u ", bitwisePrint[i]);
        WriteStream(fin);
    }
    sprintf(fin, "\n");
    WriteStream(fin);
}

extern "C" void BB_ID_Dump(uint64_t block, bool enter)
{
    char fin[128];
    uint64_t thId = threadHasher(std::this_thread::get_id());
    if (enter)
    {
        sprintf(fin, "BBEnter:%#lX:%#lX\n", block, thId);
    }
    else
    {
        sprintf(fin, "BBExit:%#lX:%#lX\n", block, thId);
    }
    WriteStream(fin);
}

extern "C" void KernelEnter(char *label)
{
    char fin[128];
    uint64_t thId = threadHasher(std::this_thread::get_id());
    sprintf(fin, "KernelEnter:%s:%#lX\n", label, thId);
    WriteStream(fin);
}
extern "C" void KernelExit(char *label)
{
    char fin[128];
    uint64_t thId = threadHasher(std::this_thread::get_id());
    sprintf(fin, "KernelExit:%s:%#lX\n", label, thId);
    WriteStream(fin);
}

#define FIFO_SIZE 1024

int taFifoRead = 0;
int taFifoWrite = 0;
char *taFifoData[FIFO_SIZE];

bool taFifoEmpty()
{
    return taFifoRead == taFifoWrite;
}

void taFifoPush(char *input)
{
    taFifoData[taFifoWrite] = input;
    taFifoWrite = (taFifoWrite + 1) % FIFO_SIZE;
}

char *taFifoPop()
{
    char* result = taFifoData[taFifoRead];
    taFifoRead = (taFifoRead + 1) % FIFO_SIZE;
    return result;
}
