#include "Backend/BackendTrace.h"
#include <assert.h>
#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#define MAX_STR 128

using namespace std;

pthread_t tracingThread;
struct sched_param param;
pthread_attr_t attr;
FILE *myfile;
pthread_mutex_t tracingMutex;
pthread_mutexattr_t tracingAttr;
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

atomic<int> taWriting = 0;

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
    param.sched_priority = SCHED_FIFO;
    pthread_setschedparam(tracingThread, SCHED_FIFO, &param);
    //pthread_setschedprio(tracingThread, SCHED_FIFO);

    initialized = true;
    while (true)
    {
        while (taFifoEmpty())
        {
            if (stopTracing)
            {

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
                break;
            }
            else
            {
                char *input = taFifoPop();
                size_t size = strlen(input);
                if (bufferIndex + size >= BUFSIZE)
                {
                    BufferData();
                }
                memcpy(storeBuffer + bufferIndex, input, size);
                //free(input);
                bufferIndex += size;
            }
        }
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
    taFifoInit();

    //param.sched_priority = SCHED_FIFO;
    //pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    //pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    //param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    //pthread_attr_setschedparam(&attr, &param);

    //pthread_create(&tracingThread, &attr, &ProcessTrace, NULL);
    pthread_create(&tracingThread, NULL, &ProcessTrace, NULL);
    while (!initialized)
    {
    }
    myfile = fopen(TraceFilename, "w");
    char suffix[MAX_STR];
    sprintf(suffix, "%s", const_cast<char *>("TraceVersion:4\n"));
    taFifoPush(suffix);
}

extern "C" void CloseFile()
{
    stopTracing = true;
    pthread_join(tracingThread, NULL);
    taFifoTerm();
    //fclose(myfile); //breaks occasionally for some reason. Likely a glibc error.
}

extern "C" void LoadDump(void *address)
{
    char fin[MAX_STR];
    uint64_t thId = pthread_self();
    sprintf(fin, "LoadAddress:%#lX:%#lX\n", (uint64_t)address, thId);
    taFifoPush(fin);
}
extern "C" void DumpLoadAddrValue(void *MemValue, int size)
{
    char fin[MAX_STR];
    uint64_t thId = pthread_self();
    sprintf(fin, "LoadAddress:%#lX\n", (uint64_t)MemValue);
    taFifoPush(fin);

    uint8_t *bitwisePrint = (uint8_t *)MemValue;
    sprintf(fin, "size:%d, LoadMemValue:", size);
    taFifoPush(fin);

    for (int i = 0; i < size; i++)
    {
        sprintf(fin, "%u ", bitwisePrint[i]);
        taFifoPush(fin);
    }
    sprintf(fin, "\n");
    taFifoPush(fin);
}
extern "C" void StoreDump(void *address)
{
    char fin[MAX_STR];
    uint64_t thId = pthread_self();
    sprintf(fin, "StoreAddress:%#lX:%#lX\n", (uint64_t)address, thId);
    taFifoPush(fin);
}

extern "C" void DumpStoreAddrValue(void *MemValue, int size)
{
    char fin[MAX_STR];
    sprintf(fin, "StoreAddress:%#lX\n", (uint64_t)MemValue);
    taFifoPush(fin);

    uint8_t *bitwisePrint = (uint8_t *)MemValue;
    sprintf(fin, "size:%d, StoreMemValue:", size);
    taFifoPush(fin);
    for (int i = 0; i < size; i++)
    {
        sprintf(fin, "%u ", bitwisePrint[i]);
        taFifoPush(fin);
    }
    sprintf(fin, "\n");
    taFifoPush(fin);
}

extern "C" void BB_ID_Dump(uint64_t block, bool enter)
{
    char fin[MAX_STR];
    uint64_t thId = pthread_self();
    if (enter)
    {
        sprintf(fin, "BBEnter:%#lX:%#lX\n", block, thId);
    }
    else
    {
        sprintf(fin, "BBExit:%#lX:%#lX\n", block, thId);
    }
    taFifoPush(fin);
}

extern "C" void KernelEnter(char *label)
{
    char fin[MAX_STR];
    uint64_t thId = pthread_self();
    sprintf(fin, "KernelEnter:%s:%#lX\n", label, thId);
    taFifoPush(fin);
}
extern "C" void KernelExit(char *label)
{
    char fin[MAX_STR];
    uint64_t thId = pthread_self();
    sprintf(fin, "KernelExit:%s:%#lX\n", label, thId);
    taFifoPush(fin);
}

#define FIFO_SIZE 1024

atomic<int> taFifoRead = 0;
atomic<int> taFifoWrite = 0;
char **taFifoData;

void taFifoInit()
{
    taFifoData = (char **)malloc(sizeof(char *) * FIFO_SIZE);
    for (int i = 0; i < FIFO_SIZE; i++)
    {
        taFifoData[i] = (char *)malloc(sizeof(char) * MAX_STR);
    }
    return;
}

void taFifoTerm()
{
    for (int i = 0; i < FIFO_SIZE; i++)
    {
        free(taFifoData[i]);
    }
    free(taFifoData);
    return;
}

bool taFifoEmpty()
{
    return taFifoRead == taFifoWrite;
}

void taFifoPush(char *input)
{
    while (taFifoFull())
    {
        usleep(100);
    };
    pthread_mutex_lock(&tracingMutex);
    strcpy(taFifoData[taFifoWrite], input);
    taFifoWrite = (taFifoWrite + 1) % FIFO_SIZE;
    pthread_mutex_unlock(&tracingMutex);
    return;
}

char *taFifoPop()
{
    pthread_mutex_lock(&tracingMutex);
    char *result = taFifoData[taFifoRead];
    taFifoRead = (taFifoRead + 1) % FIFO_SIZE;
    pthread_mutex_unlock(&tracingMutex);
    return result;
}

bool taFifoFull()
{
    int dif = taFifoRead - taFifoWrite;
    return dif == -1 || dif == FIFO_SIZE - 1;
}
