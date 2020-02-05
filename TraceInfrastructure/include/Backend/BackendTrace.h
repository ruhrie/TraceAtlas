#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string>

/// <summary>
/// Writes the input string to the trace buffer.
/// </summary>
/// <param name="input">The string to be written</param>
void WriteStream();

/// <summary>
/// Compresses the trace buffer and writes it to the destination file.
/// </summary>
void BufferData();

/// <summary>
/// Writes the input instruction and line, block, and function UIDs to the trace buffer.
/// </summary>
/// <param name="inst">A stromg representing the instruction to write.</param>
/// <param name="line">The line UID.</param>
/// <param name="block">The block UID</param>
/// <param name="func">The function GUID.</param>
void Write(char *inst, int line, int block, uint64_t func);

/// <summary>
/// Writes the input instruction, line, block, function UIDs, and the memory address to the trace buffer.
/// </summary>
/// <param name="inst">A stromg representing the instruction to write.</param>
/// <param name="line">The line UID.</param>
/// <param name="block">The block UID</param>
/// <param name="func">The function GUID.</param>
/// <param name="address">A byte array representing a system pointer.</param>
void WriteAddress(char *inst, int line, int block, uint64_t func, char *address);

/// <summary>
/// Opens the trace file for writing and initializes the compression stream.
/// </summary>
/// <param name="fileName">The string to be written</param>
/// <param name="compressionLevel">The zlib compression level to be used.</param>
extern "C" void OpenFile();

/// <summary>
/// Flushes the compression stream and closes the trace file.
/// </summary>
extern "C" void CloseFile();

extern "C" void LoadDump(void *address);
extern "C" void DumpLoadAddrValue(void *MemValue, int size);
extern "C" void StoreDump(void *address);
extern "C" void DumpStoreAddrValue(void *MemValue, int size);

extern "C" void BB_ID_Dump(uint64_t block, bool enter);

extern "C" void KernelEnter(char *label);
extern "C" void KernelExit(char *label);

bool taFifoEmpty();
bool taFifoFull();
char *taFifoPush();
char *taFifoPop();
void taFifoInit();
void taFifoTerm();