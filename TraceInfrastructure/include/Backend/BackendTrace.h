#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
    /// <summary>
    /// Compresses the trace buffer and writes it to the destination file.
    /// </summary>
    void BufferData();

    /// <summary>
    /// Opens the trace file for writing and initializes the compression stream.
    /// </summary>
    /// <param name="fileName">The string to be written</param>
    /// <param name="compressionLevel">The zlib compression level to be used.</param>
    void OpenFile();
    /// <summary>
    /// Writes the input string to the trace buffer.
    /// </summary>
    /// <param name="input">The string to be written</param>
    void WriteStream(char *input);
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
    /// Flushes the compression stream and closes the trace file.
    /// </summary>
    void CloseFile();

    void LoadDump(void *address);
    void DumpLoadAddrValue(void *MemValue, int size);
    void StoreDump(void *address);
    void DumpStoreAddrValue(void *MemValue, int size);

    void BB_ID_Dump(uint64_t block, bool enter);

    void KernelEnter(char *label);
    void KernelExit(char *label);
#ifdef __cplusplus
}
#endif