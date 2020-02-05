#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string>

/// <summary>
/// Compresses the trace buffer and writes it to the destination file.
/// </summary>
void BufferData();

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
void taFifoPush(char* input);
char *taFifoPop();
void taFifoInit();
void taFifoTerm();