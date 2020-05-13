#include "Backend/BackendTrace.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#define THREAD_MAX 127

//std::array<FILE *, THREAD_MAX> myfiles;
FILE *myfiles[THREAD_MAX];
//trace functions
//std::array<z_stream, THREAD_MAX> strm_DashTracers;
z_stream strm_DashTracers[THREAD_MAX];

int TraceCompressionLevel;
char *TraceFilename;
/// <summary>
/// The maximum ammount of bytes to store in a buffer before flushing it.
/// </summary>
#define BUFSIZE 128 * 1024

uint32_t bufferIndeces[THREAD_MAX];

atomic_char nameIndex = 0;

//std::array<std::array<uint8_t, BUFSIZE>, THREAD_MAX> storeBuffers;
uint8_t storeBuffers[THREAD_MAX][BUFSIZE];
//std::array<std::array<uint8_t, BUFSIZE>, THREAD_MAX> tempBuffers;
uint8_t tempBuffers[THREAD_MAX][BUFSIZE];

_Thread_local uint64_t bid = 0;

_Thread_local int8_t threadId = -1;

void OpenFile()
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
    strm_DashTracers[threadId].zalloc = Z_NULL;
    strm_DashTracers[threadId].zfree = Z_NULL;
    strm_DashTracers[threadId].opaque = Z_NULL;
    deflateInit(&strm_DashTracers[threadId], TraceCompressionLevel);
    char *tfn = getenv("TRACE_NAME");
    if (tfn != NULL)
    {
        TraceFilename = tfn;
    }
    else
    {
        TraceFilename = "raw.trc";
    }
    char finalName[128];
    sprintf(finalName, "%s.%d", TraceFilename, threadId);
    myfiles[threadId] = fopen(finalName, "wb");
    WriteStream("TraceVersion:3\n");
}

///Modified from https://stackoverflow.com/questions/4538586/how-to-compress-a-buffer-with-zlib
void BufferData(int8_t index)
{
    strm_DashTracers[index].next_in = storeBuffers[index];
    strm_DashTracers[index].avail_in = bufferIndeces[index];
    strm_DashTracers[index].next_out = tempBuffers[index];
    strm_DashTracers[index].avail_out = BUFSIZE;
    while (strm_DashTracers[index].avail_in != 0)
    {
        deflate(&strm_DashTracers[index], Z_NO_FLUSH);

        if (strm_DashTracers[index].avail_out == 0)
        {
            fwrite(tempBuffers[index], sizeof(uint8_t), BUFSIZE - strm_DashTracers[index].avail_out, myfiles[index]);
            strm_DashTracers[index].next_out = tempBuffers[index];
            strm_DashTracers[index].avail_out = BUFSIZE;
        }
    }
    fwrite(tempBuffers[index], sizeof(uint8_t), BUFSIZE - strm_DashTracers[index].avail_out, myfiles[index]);
    strm_DashTracers[index].next_out = tempBuffers[index];
    strm_DashTracers[index].avail_out = BUFSIZE;
    bufferIndeces[index] = 0;
}

void WriteStream(char *input)
{
    size_t size = strlen(input);
    if (threadId == -1)
    {
        threadId = nameIndex++;
        OpenFile();
    }
    if (bufferIndeces[threadId] + size >= BUFSIZE)
    {
        BufferData(threadId);
    }

    memcpy(storeBuffers[threadId] + bufferIndeces[threadId], input, size);
    bufferIndeces[threadId] += size;
}
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

void CloseFile()
{
    for (int8_t i = 0; i < nameIndex; i++)
    {
        BufferData(i);
        strm_DashTracers[i].next_in = storeBuffers[i];
        strm_DashTracers[i].avail_in = bufferIndeces[i];
        strm_DashTracers[i].next_out = tempBuffers[i];
        strm_DashTracers[i].avail_out = BUFSIZE;
        deflate(&strm_DashTracers[i], Z_FINISH);
        fwrite(tempBuffers[i], sizeof(uint8_t), BUFSIZE - strm_DashTracers[i].avail_out, myfiles[i]);
        deflateEnd(&strm_DashTracers[i]);
        fclose(myfiles[i]);
    }
}

void LoadDump(void *address)
{
    char fin[128];
    struct timespec tr;
    clock_gettime(CLOCK_MONOTONIC, &tr);
    int64_t t = tr.tv_sec * 1000000000 + tr.tv_nsec;
    sprintf(fin, "LoadAddress:%#lX:%#lX\n", (uint64_t)address, (uint64_t)t);
    WriteStream(fin);
}
void DumpLoadValue(void *MemValue, int size)
{
    char fin[128];
    struct timespec tr;
    clock_gettime(CLOCK_MONOTONIC, &tr);
    int64_t t = tr.tv_sec * 1000000000 + tr.tv_nsec;
    sprintf(fin, "LoadAddress:%#lX:%#lX\n", (uint64_t)MemValue, (uint64_t)t);
    WriteStream(fin);
    uint8_t *bitwisePrint = (uint8_t *)MemValue;
    sprintf(fin, "LoadValue:");
    WriteStream(fin);
    for (int i = 0; i < size; i++)
    {
        if (i == 0)
        {
            sprintf(fin, "0X%02X", bitwisePrint[i]);
        }
        else
        {
            sprintf(fin, "%02X", bitwisePrint[i]);
        }
        WriteStream(fin);
    }
    sprintf(fin, "\n");
    WriteStream(fin);
}
void StoreDump(void *address)
{
    char fin[128];
    struct timespec tr;
    clock_gettime(CLOCK_MONOTONIC, &tr);
    int64_t t = tr.tv_sec * 1000000000 + tr.tv_nsec;
    sprintf(fin, "StoreAddress:%#lX:%#lX\n", (uint64_t)address, (uint64_t)t);
    WriteStream(fin);
}

void DumpStoreValue(void *MemValue, int size)
{
    char fin[128];
    struct timespec tr;
    clock_gettime(CLOCK_MONOTONIC, &tr);
    int64_t t = tr.tv_sec * 1000000000 + tr.tv_nsec;
    sprintf(fin, "StoreAddress:%#lX:%#lX\n", (uint64_t)MemValue, (uint64_t)t);
    WriteStream(fin);
    uint8_t *bitwisePrint = (uint8_t *)MemValue;
    sprintf(fin, "StoreValue:");
    WriteStream(fin);
    for (int i = 0; i < size; i++)
    {
        if (i == 0)
        {
            sprintf(fin, "0X%02X", bitwisePrint[i]);
        }
        else
        {
            sprintf(fin, "%02X", bitwisePrint[i]);
        }
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
