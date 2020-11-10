#include "Backend/BackendTrace.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

FILE *TraceAtlasTraceFile;

//trace functions
z_stream TraceAtlasStrm;

int TraceAtlasTraceCompressionLevel;
char *TraceAtlasTraceFilename;
/// <summary>
/// The maximum ammount of bytes to store in a buffer before flushing it.
/// </summary>
const uint32_t TRACEATLASBUFSIZE = 128 * 1024;
unsigned int TraceAtlasBufferIndex = 0;
uint8_t TraceAtlasTempBuffer[TRACEATLASBUFSIZE];
uint8_t TraceAtlasStoreBuffer[TRACEATLASBUFSIZE];

bool TraceAtlasOpened = false;
bool TraceAtlasClosed = false;

void TraceAtlasWriteStream(char *input)
{
    size_t size = strlen(input);
    if (TraceAtlasBufferIndex + size >= TRACEATLASBUFSIZE)
    {
        TraceAtlasBufferData();
    }
    memcpy(TraceAtlasStoreBuffer + TraceAtlasBufferIndex, input, size);
    TraceAtlasBufferIndex += size;
}

///Modified from https://stackoverflow.com/questions/4538586/how-to-compress-a-buffer-with-zlib
void TraceAtlasBufferData()
{
    if (TraceAtlasTraceCompressionLevel != -2)
    {
        TraceAtlasStrm.next_in = TraceAtlasStoreBuffer;
        TraceAtlasStrm.avail_in = TraceAtlasBufferIndex;
        while (TraceAtlasStrm.avail_in != 0)
        {
            int defResult = deflate(&TraceAtlasStrm, Z_PARTIAL_FLUSH);
            if (defResult != Z_OK)
            {
                fprintf(stderr, "Zlib compression error");
                exit(-1);
            }
            if (TraceAtlasStrm.avail_out == 0)
            {
                fwrite(TraceAtlasTempBuffer, sizeof(Bytef), TRACEATLASBUFSIZE, TraceAtlasTraceFile);
                TraceAtlasStrm.next_out = TraceAtlasTempBuffer;
                TraceAtlasStrm.avail_out = TRACEATLASBUFSIZE;
            }
        }
    }
    else
    {

        fwrite(TraceAtlasStoreBuffer, sizeof(char), TraceAtlasBufferIndex, TraceAtlasTraceFile);
    }
    TraceAtlasBufferIndex = 0;
}

void TraceAtlasWrite(char *inst, int line, int block, uint64_t func)
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
    TraceAtlasWriteStream(fin);
}

void TraceAtlasWriteAddress(char *inst, int line, int block, uint64_t func, char *address)
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
    TraceAtlasWriteStream(fin);
}

void TraceAtlasOpenFile()
{
    if (!TraceAtlasOpened)
    {
        char *tcl = getenv("TRACE_COMPRESSION");
        if (tcl != NULL)
        {
            int l = atoi(tcl);
            TraceAtlasTraceCompressionLevel = l;
        }
        else
        {
            TraceAtlasTraceCompressionLevel = Z_DEFAULT_COMPRESSION;
        }
        if (TraceAtlasTraceCompressionLevel != -2)
        {
            TraceAtlasStrm.zalloc = Z_NULL;
            TraceAtlasStrm.zfree = Z_NULL;
            TraceAtlasStrm.opaque = Z_NULL;
            TraceAtlasStrm.next_out = TraceAtlasTempBuffer;
            TraceAtlasStrm.avail_out = TRACEATLASBUFSIZE;
            int defResult = deflateInit(&TraceAtlasStrm, TraceAtlasTraceCompressionLevel);
            if (defResult != Z_OK)
            {
                fprintf(stderr, "Zlib compression error");
                exit(-1);
            }
        }
        char *tfn = getenv("TRACE_NAME");
        if (tfn != NULL)
        {
            TraceAtlasTraceFilename = tfn;
        }
        else
        {
            TraceAtlasTraceFilename = "raw.trc";
        }

        TraceAtlasTraceFile = fopen(TraceAtlasTraceFilename, "w");
        TraceAtlasWriteStream("TraceVersion:3\n");
        TraceAtlasOpened = true;
    }
}

void TraceAtlasCloseFile()
{
    if (!TraceAtlasClosed)
    {
        if (TraceAtlasTraceCompressionLevel != -2)
        {
            TraceAtlasStrm.next_in = TraceAtlasStoreBuffer;
            TraceAtlasStrm.avail_in = TraceAtlasBufferIndex;
            while (true)
            {
                int defRes = deflate(&TraceAtlasStrm, Z_FINISH);
                if (defRes == Z_BUF_ERROR)
                {
                    fprintf(stderr, "Zlib buffer error");
                    exit(-1);
                }
                else if (defRes == Z_STREAM_ERROR)
                {
                    fprintf(stderr, "Zlib stream error");
                    exit(-1);
                }
                if (TraceAtlasStrm.avail_out == 0)
                {
                    fwrite(TraceAtlasTempBuffer, sizeof(Bytef), TRACEATLASBUFSIZE, TraceAtlasTraceFile);
                    TraceAtlasStrm.next_out = TraceAtlasTempBuffer;
                    TraceAtlasStrm.avail_out = TRACEATLASBUFSIZE;
                }
                if (defRes == Z_STREAM_END)
                {
                    break;
                }
            }
            fwrite(TraceAtlasTempBuffer, sizeof(Bytef), TRACEATLASBUFSIZE - TraceAtlasStrm.avail_out, TraceAtlasTraceFile);

            deflateEnd(&TraceAtlasStrm);
        }
        else
        {
            fwrite(TraceAtlasStoreBuffer, sizeof(Bytef), TraceAtlasBufferIndex, TraceAtlasTraceFile);
        }

        TraceAtlasClosed = true;
        //fclose(myfile); //breaks occasionally for some reason. Likely a glibc error.
    }
}

void TraceAtlasLoadDump(void *address)
{
    char fin[128];
    sprintf(fin, "LoadAddress:%#lX\n", (uint64_t)address);
    TraceAtlasWriteStream(fin);
}
void TraceAtlasDumpLoadValue(void *MemValue, int size)
{
    char fin[128];
    uint8_t *bitwisePrint = (uint8_t *)MemValue;
    sprintf(fin, "LoadValue:");
    TraceAtlasWriteStream(fin);
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
        TraceAtlasWriteStream(fin);
    }
    sprintf(fin, "\n");
    TraceAtlasWriteStream(fin);
}
void TraceAtlasStoreDump(void *address)
{
    char fin[128];
    sprintf(fin, "StoreAddress:%#lX\n", (uint64_t)address);
    TraceAtlasWriteStream(fin);
}

void TraceAtlasDumpStoreValue(void *MemValue, int size)
{
    char fin[128];
    uint8_t *bitwisePrint = (uint8_t *)MemValue;
    sprintf(fin, "StoreValue:");
    TraceAtlasWriteStream(fin);
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
        TraceAtlasWriteStream(fin);
    }
    sprintf(fin, "\n");
    TraceAtlasWriteStream(fin);
}

void TraceAtlasBB_ID_Dump(uint64_t block, bool enter)
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
    TraceAtlasWriteStream(fin);
}

void TraceAtlasKernelEnter(char *label)
{
    char fin[128];
    strcpy(fin, "KernelEnter:");
    strcat(fin, label);
    strcat(fin, "\n");
    TraceAtlasWriteStream(fin);
}
void TraceAtlasKernelExit(char *label)
{
    char fin[128];
    strcpy(fin, "KernelExit:");
    strcat(fin, label);
    strcat(fin, "\n");
    TraceAtlasWriteStream(fin);
}
