#include "Backend/BackendTrace.h"
#include <assert.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <zlib.h>

FILE *myfile;
std::mutex tracingMutex;
pthread_mutexattr_t tracingAttr;
std::hash<std::thread::id> threadHasher;

//trace functions
z_stream strm_DashTracer;

int TraceCompressionLevel;
char *TraceFilename;
/// <summary>
/// The maximum ammount of bytes to store in a buffer before flushing it.
/// </summary>
#define BUFSIZE 128 * 1024
unsigned int bufferIndex = 0;
uint8_t temp_buffer[BUFSIZE];
uint8_t storeBuffer[BUFSIZE];

void WriteStream(char *input)
{
    size_t size = strlen(input);
    tracingMutex.lock();
    //pthread_mutex_lock(&tracingMutex);
    uint64_t thId = threadHasher(std::this_thread::get_id());
    if (bufferIndex + size >= BUFSIZE)
    {
        BufferData();
    }
    memcpy(storeBuffer + bufferIndex, input, size);
    bufferIndex += size;
    tracingMutex.unlock();
    //pthread_mutex_unlock(&tracingMutex);
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
    char *tfn = getenv("TRACE_NAME");
    if (tfn != NULL)
    {
        TraceFilename = tfn;
    }
    else
    {
        TraceFilename = const_cast<char *>("raw.trc");
    }

    //pthread_mutex_init(&tracingMutex, &tracingAttr);
    myfile = fopen(TraceFilename, "w");
    WriteStream(const_cast<char *>("TraceVersion:4\n"));
}

extern "C" void CloseFile()
{
    strm_DashTracer.next_in = storeBuffer;
    strm_DashTracer.avail_in = bufferIndex;
    strm_DashTracer.next_out = temp_buffer;
    strm_DashTracer.avail_out = BUFSIZE;
    int deflate_res = deflate(&strm_DashTracer, Z_FINISH);
    assert(deflate_res == Z_STREAM_END);
    for (int i = 0; i < BUFSIZE - strm_DashTracer.avail_out; i++)
    {
        fputc(temp_buffer[i], myfile);
    }

    deflateEnd(&strm_DashTracer);
    //pthread_mutex_destroy(&tracingMutex);
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