#pragma once
#include <stdbool.h>
#include <stdint.h>

void MemInstructionDump(void *address, uint64_t bbID, uint64_t datasize, uint64_t type)
void MemProfInitialization()
void MemProfDestroy()