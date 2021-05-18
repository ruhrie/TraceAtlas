#pragma once
#include <stdbool.h>
#include <stdint.h>

/// <summary>
/// Writes the input string to the trace buffer.
/// </summary>
/// <param name="input">The string to be written</param>
void TraceAtlasWriteStream(char *input);

/// <summary>
/// Compresses the trace buffer and writes it to the destination file.
/// </summary>
void TraceAtlasBufferData();

/// <summary>
/// Writes the input instruction and line, block, and function UIDs to the trace buffer.
/// </summary>
/// <param name="inst">A stromg representing the instruction to write.</param>
/// <param name="line">The line UID.</param>
/// <param name="block">The block UID</param>
/// <param name="func">The function GUID.</param>
void TraceAtlasWrite(char *inst, int line, int block, uint64_t func);

/// <summary>
/// Writes the input instruction, line, block, function UIDs, and the memory address to the trace buffer.
/// </summary>
/// <param name="inst">A stromg representing the instruction to write.</param>
/// <param name="line">The line UID.</param>
/// <param name="block">The block UID</param>
/// <param name="func">The function GUID.</param>
/// <param name="address">A byte array representing a system pointer.</param>
void TraceAtlasWriteAddress(char *inst, int line, int block, uint64_t func, char *address);

/// <summary>
/// Opens the trace file for writing and initializes the compression stream.
/// </summary>
void TraceAtlasOpenFile();

/// <summary>
/// Flushes the compression stream and closes the trace file.
/// </summary>
void TraceAtlasCloseFile();

void TraceAtlasLoadDump(void *address);
void TraceAtlasDumpLoadValue(void *MemValue, int size);
void TraceAtlasStoreDump(void *address);
void TraceAtlasDumpStoreValue(void *MemValue, int size);

void BB_ID_Dump(uint64_t block, bool enter);

#ifdef __cplusplus
extern "C"
{
#endif
    void TraceAtlasKernelEnter(char *label);
    void TraceAtlasKernelExit(char *label);
    void TraceAtlasMarkovKernelEnter(char *label);
    void TraceAtlasMarkovKernelExit(char *label);
#ifdef __cplusplus
}
#endif