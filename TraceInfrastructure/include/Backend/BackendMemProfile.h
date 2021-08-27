#pragma once
#include <stdbool.h>
#include <stdint.h>


void LoadInstructionDump(void *address, uint64_t bbID, uint64_t datasize)
void StoreInstructionDump(void *address, uint64_t bbID, uint64_t datasize)
void MemProfInitialization()
void MemProfDestroy()