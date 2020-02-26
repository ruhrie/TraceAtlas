#pragma once
#include <stdbool.h>
#include <stdint.h>

/// <summary>
/// Writes the input string to the trace buffer.
/// </summary>
/// <param name="input">The string to be written</param>
void WriteStream(char *input);

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

void BB_ID_Dump(uint64_t block, bool enter);

