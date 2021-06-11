#include "Backend/DashHashTable.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// 2^20 basic blocks is about the worst case scenario we can expect
#define HASHTABLESIZE 1048576
// when the array size and entries is 2^20, 8 is the first number where we start seeing clashes
#define AVG_NEIGHBORS 25
// when the array size and entries is 2^20 and the average neighbor size is 8, 2^19 is the first number where we start seeing clashes
#define HIGHLY_CONNECTED_NEIGHBORS 10000000

// This program pushes a ton of entries into the hash table causing a clash resolution, then checks if all entries have been preserved
// Since all newly allocated entries are initialized to 0, there is a corner case here where an entry with source 0 and sink 0 has frequency 0 and exists in the hash table everywhere an entry hasn't been touched. When the clashResolve writes all the old stuff into the new stuff, it comes across these entries and writes the wrong value to the 0,0 entry (if that entry has a non-zero frequency)
void checkAccuracy(__TA_HashTable *a, int i, int l)
{
    __TA_edgeTuple entry;
    // check the first for loop which should already be in there
    for (int j = 0; j <= i; j++)
    {
        entry.source = j;
        entry.frequency = 1; // this should be the value of the frequency because every entry is pushed by the incrementer
        for (int k = 0; k < AVG_NEIGHBORS; k++)
        {
            // case where j is on the last entry and we fail on neighbor l
            if (j == i)
            {
                if (k == l)
                {
                    break;
                }
            }
            entry.sink = j + k;
            __TA_edgeTuple *read = __TA_HashTable_read(a, &entry);
            if (!read)
            {
                printf("Failed to recover an entry of nodes (%d,%d) that should exist!\n", entry.source, entry.sink);
            }
            else if (read->frequency != entry.frequency)
            {
                printf("The frequency value for entry (%d,%d) was %lu and the correct answer was %lu!\n", read->source, read->sink, read->frequency, entry.frequency);
            }
        }
    }
}

void checkAccuracy2(__TA_HashTable *a, int i)
{
    __TA_edgeTuple entry;
    entry.source = HASHTABLESIZE - 1;
    entry.frequency = 1; // because each neighbor is pushed with an increment
    for (int k = 0; k < i; k++)
    {
        entry.sink = k;
        __TA_edgeTuple *read = __TA_HashTable_read(a, &entry);
        if (!read)
        {
            printf("Failed to recover an entry of nodes (%d,%d) that should exist!\n", entry.source, entry.sink);
        }
        else if (read->frequency != entry.frequency)
        {
            printf("The frequency value for entry (%d,%d) was %lu and the correct answer was %lu!\n", read->source, read->sink, read->frequency, entry.frequency);
        }
    }
}

int main()
{
    // initial allocation of all data structures
    __TA_HashTable *hashTable = (__TA_HashTable *)malloc(sizeof(__TA_HashTable));
    // convert array size to power of 2, round to the ceiling
    hashTable->size = (uint32_t)(ceil(log((double)HASHTABLESIZE) / log(2.0)));
    hashTable->getFullSize = __TA_getFullSize;
    hashTable->array = (__TA_arrayElem *)malloc((hashTable->getFullSize(hashTable)) * sizeof(__TA_arrayElem));
    __TA_edgeTuple entry0;

    // this pushes AVG_NEIGHBORS * HASHTABLESIZE entries into the table
    for (int i = 0; i < HASHTABLESIZE - 1; i++)
    {
        entry0.source = i;
        entry0.frequency = 0;
        for (int j = 0; j < AVG_NEIGHBORS; j++)
        {
            entry0.sink = i + j;
            while (__TA_HashTable_increment(hashTable, &entry0))
            {
                __TA_resolveClash(hashTable);
                checkAccuracy(hashTable, i, j);
            }
        }
    }

    printf("Now starting the highly connected node\n");
    // this pushes 2^18 entries into the table
    entry0.source = HASHTABLESIZE - 1;
    entry0.frequency = 0;
    for (int i = 0; i < HIGHLY_CONNECTED_NEIGHBORS; i++)
    {
        entry0.sink = i;
        while (__TA_HashTable_increment(hashTable, &entry0))
        {
            __TA_resolveClash(hashTable);
            checkAccuracy2(hashTable, i);
        }
    }

    free(hashTable->array);
    free(hashTable);
    return 0;
}
