#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "Backend/DashHashTable.h"

#define HASHTABLESIZE 1024

// test cases
// 1. make a fully connected graph and each edge has 1024 frequency
// 2. make a sliding window where the window is fully connected
// 3. use the iterator to test the read function
//
// Sanity check: use functions in the profiler that check whether or not the problem is with me or with the program

int main()
{
    // initial allocation of all data structures
    __TA_HashTable* hashTable = (__TA_HashTable*)malloc( sizeof(__TA_HashTable) );
    // convert array size to power of 2, round to the ceiling
    hashTable->size = (uint32_t)( ceil( log((double)HASHTABLESIZE) / log(2.0) ) );
    hashTable->getFullSize = getFullSize;
    hashTable->array = (__TA_arrayElem*)malloc( (hashTable->getFullSize(hashTable))*sizeof(__TA_arrayElem) );
    __TA_kvTuple entry0;

	entry0.source    = 0;
	entry0.sink      = 1;
	entry0.frequency = 0;
    __TA_HashTable_increment(hashTable, &entry0);
    __TA_kvTuple* read = __TA_HashTable_read(hashTable, &entry0);
    printf("The entry for source node %d has sink node %d and frequency count %lu.\n", read->source, read->sink, read->frequency);

    entry0.frequency = 100;
    __TA_HashTable_write(hashTable, &entry0);
    read = __TA_HashTable_read(hashTable, &entry0);
    printf("The entry for source node %d has sink node %d and frequency count %lu.\n", read->source, read->sink, read->frequency);

    __TA_HashTable_increment(hashTable, &entry0);
    read = __TA_HashTable_read(hashTable, &entry0);
    printf("The entry for source node %d has sink node %d and frequency count %lu.\n", read->source, read->sink, read->frequency);

    free(hashTable->array);
    free(hashTable);
    return 0;
}
