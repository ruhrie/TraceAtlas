#pragma once
#include <stdbool.h>
#include <stdint.h>

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
/// Opens the trace file for writing and initializes the compression stream.
/// </summary>
/// <param name="fileName">The string to be written</param>
/// <param name="compressionLevel">The zlib compression level to be used.</param>
void OpenFile();

/// <summary>
/// Flushes the compression stream and closes the trace file.
/// </summary>
void CloseFile();

void BB_ID_Dump(uint64_t block, bool enter);

