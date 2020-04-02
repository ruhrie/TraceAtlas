#include "Backend/BackendTrace.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <zlib.h>

using namespace std;

FILE *myfile;

//trace functions
z_stream strm_DashTracer;

int TraceCompressionLevel;
char *TraceFilename;
/// <summary>
/// The maximum ammount of bytes to store in a buffer before flushing it.
/// </summary>
#define BUFSIZE 128 * 1024
unsigned int bufferIndex = 0;
std::array<uint8_t, BUFSIZE> tempBuffer;
std::array<uint8_t, BUFSIZE> storeBuffer;

pthread_t workerThread;
pthread_mutex_t mutex;
pthread_cond_t cond;
bool shouldExit = false;

void *Worker(void *param)
{
    while (true)
    {
        pthread_mutex_lock(&mutex);
        BufferData();
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
        if (shouldExit)
        {
            break;
        }
    }
    return nullptr;
}

void WriteStream(char *input)
{
    size_t size = strlen(input);
    if (bufferIndex + size >= BUFSIZE)
    {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_lock(&mutex);
    memcpy(storeBuffer.begin() + bufferIndex, input, size);
    bufferIndex += size;
    pthread_mutex_unlock(&mutex);
}

///Modified from https://stackoverflow.com/questions/4538586/how-to-compress-a-buffer-with-zlib
void BufferData()
{
    strm_DashTracer.next_in = storeBuffer.begin();
    strm_DashTracer.avail_in = bufferIndex;
    strm_DashTracer.next_out = tempBuffer.begin();
    strm_DashTracer.avail_out = BUFSIZE;
    while (strm_DashTracer.avail_in != 0)
    {
        deflate(&strm_DashTracer, Z_NO_FLUSH);

        if (strm_DashTracer.avail_out == 0)
        {
            fwrite(tempBuffer.data(), sizeof(uint8_t), BUFSIZE - strm_DashTracer.avail_out, myfile);
            strm_DashTracer.next_out = tempBuffer.begin();
            strm_DashTracer.avail_out = BUFSIZE;
        }
    }
    fwrite(tempBuffer.data(), sizeof(uint8_t), BUFSIZE - strm_DashTracer.avail_out, myfile);
    strm_DashTracer.next_out = tempBuffer.begin();
    strm_DashTracer.avail_out = BUFSIZE;
    bufferIndex = 0;
}

extern "C"
{
    void Write(char *inst, int line, int block, uint64_t func)
    {
        char suffix[128];
        sprintf(suffix, ";line:%d;block:%d;function:%lu\n", line, block, func);
        size_t size = strlen(inst) + strlen(suffix);
        char fin[size];
        strcpy(fin, inst);
        strncat(fin, suffix, 128);
        WriteStream(fin);
    }

    void WriteAddress(char *inst, int line, int block, uint64_t func, char *address)
    {
        char suffix[128];
        sprintf(suffix, ";line:%d;block:%d;function:%lu;address:%lu\n", line, block, func, (uint64_t)address);
        size_t size = strlen(inst) + strlen(suffix);
        char fin[size];

        strcpy(fin, inst);
        strncat(fin, suffix, 128);
        WriteStream(fin);
    }

    void OpenFile()
    {
        char *tcl = getenv("TRACE_COMPRESSION");
        if (tcl != nullptr)
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
        deflateInit(&strm_DashTracer, TraceCompressionLevel);
        char *tfn = getenv("TRACE_NAME");
        if (tfn != nullptr)
        {
            TraceFilename = tfn;
        }
        else
        {
            TraceFilename = "raw.trc";
        }
        myfile = fopen(TraceFilename, "wb");
        pthread_create(&workerThread, nullptr, Worker, nullptr);
        WriteStream("TraceVersion:3\n");
    }

    void CloseFile()
    {
        shouldExit = true;
        pthread_join(workerThread, nullptr);
        strm_DashTracer.next_in = storeBuffer.begin();
        strm_DashTracer.avail_in = bufferIndex;
        strm_DashTracer.next_out = tempBuffer.begin();
        strm_DashTracer.avail_out = BUFSIZE;
        deflate(&strm_DashTracer, Z_FINISH);
        fwrite(tempBuffer.data(), sizeof(uint8_t), BUFSIZE - strm_DashTracer.avail_out, myfile);

        deflateEnd(&strm_DashTracer);
        //fclose(myfile); //breaks occasionally for some reason. Likely a glibc error.
    }

    void LoadDump(void *address)
    {
        char fin[128];
        sprintf(fin, "LoadAddress:%#lX\n", (uint64_t)address);
        WriteStream(fin);
    }
    void DumpLoadAddrValue(void *MemValue, int size)
    {
        char fin[128];
        sprintf(fin, "LoadAddress:%#lX\n", (uint64_t)MemValue);
        WriteStream(fin);
        auto bitwisePrint = (uint8_t *)MemValue;
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
    void StoreDump(void *address)
    {
        char fin[128];
        sprintf(fin, "StoreAddress:%#lX\n", (uint64_t)address);
        WriteStream(fin);
    }

    void DumpStoreAddrValue(void *MemValue, int size)
    {
        char fin[128];
        sprintf(fin, "StoreAddress:%#lX\n", (uint64_t)MemValue);
        WriteStream(fin);
        auto bitwisePrint = (uint8_t *)MemValue;
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

    void BB_ID_Dump(uint64_t block, bool enter)
    {
        char fin[128];
        if (enter)
        {
            sprintf(fin, "BBEnter:%#lX\n", block);
        }
        else
        {
            sprintf(fin, "BBExit:%#lX\n", block);
        }
        WriteStream(fin);
    }

    void KernelEnter(char *label)
    {
        char fin[128];
        strcpy(fin, "KernelEnter:");
        strcat(fin, label);
        strcat(fin, "\n");
        WriteStream(fin);
    }
    void KernelExit(char *label)
    {
        char fin[128];
        strcpy(fin, "KernelExit:");
        strcat(fin, label);
        strcat(fin, "\n");
        WriteStream(fin);
    }
}