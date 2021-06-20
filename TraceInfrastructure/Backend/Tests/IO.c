#include "Backend/DashHashTable.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// 2^20 basic blocks is about the worst case scenario we can expect
#define HASHTABLESIZE 1048576
// when the array size and entries is 2^20, 8 is the first number where we start seeing clashes
#define AVG_NEIGHBORS 15
// when the array size and entries is 2^20 and the average neighbor size is 8, 2^19 is the first number where we start seeing clashes
#define HIGHLY_CONNECTED_NEIGHBORS 10000
// This program tests the IO of the hashtable program
// 1. It pushes a ton of entries into the hash table
// 2. Checks if all entries have been preserved

void CheckFileAccuracy(__TA_HashTable *a, __TA_HashTable *b)
{
    for (uint32_t i = 0; i < a->getFullSize(a); i++)
    {
        for (uint32_t j = 0; j < a->array[i].popCount; j++)
        {
            __TA_edgeTuple old = a->array[i].tuple[j].edge;
            __TA_edgeTuple new = b->array[i].tuple[j].edge;
            if (old.blocks[0] != new.blocks[0] || old.blocks[1] != new.blocks[1] || old.frequency != new.frequency)
            {
                printf("Found an entry that did not match! old: (%d,%d,%lu), new: (%d,%d,%lu)\n", old.blocks[0], old.blocks[1], old.frequency, new.blocks[0], new.blocks[1], new.frequency);
            }
        }
    }
}

void checkAccuracy(__TA_HashTable *a, int i, int l)
{
    __TA_edgeTuple entry;
    // check the first for loop which should already be in there
    for (int j = 0; j <= i; j++)
    {
        entry.blocks[0] = j;
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
            entry.blocks[1] = j + k;
            __TA_element *read = __TA_HashTable_read(a, (__TA_element *)&entry);
            if (!read)
            {
                printf("Failed to recover an entry of nodes (%d,%d) that should exist!\n", entry.blocks[0], entry.blocks[1]);
            }
            else if (read->edge.frequency != entry.frequency)
            {
                printf("The frequency value for entry (%d,%d) was %lu and the correct answer was %lu!\n", read->edge.blocks[0], read->edge.blocks[1], read->edge.frequency, entry.frequency);
            }
        }
    }
}

void checkAccuracy2(__TA_HashTable *a, int i)
{
    __TA_edgeTuple entry;
    entry.blocks[0] = HASHTABLESIZE - 1;
    entry.frequency = 1; // because each neighbor is pushed with an increment
    for (int k = 0; k < i; k++)
    {
        entry.blocks[1] = k;
        __TA_element *read = __TA_HashTable_read(a, (__TA_element *)&entry);
        if (!read)
        {
            printf("Failed to recover an entry of nodes (%d,%d) that should exist!\n", entry.blocks[0], entry.blocks[1]);
        }
        else if (read->edge.frequency != entry.frequency)
        {
            printf("The frequency value for entry (%d,%d) was %lu and the correct answer was %lu!\n", read->edge.blocks[0], read->edge.blocks[1], read->edge.frequency, entry.frequency);
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
        entry0.blocks[0] = i;
        entry0.frequency = 0;
        for (int j = 0; j < AVG_NEIGHBORS; j++)
        {
            entry0.blocks[1] = i + j;
            while (__TA_HashTable_increment(hashTable, (__TA_element *)&entry0))
            {
                __TA_resolveClash(hashTable, hashTable->size + 1);
            }
        }
    }

    printf("Now starting the highly connected node\n");
    entry0.blocks[0] = HASHTABLESIZE - 1;
    entry0.frequency = 0;
    for (int i = 0; i < HIGHLY_CONNECTED_NEIGHBORS; i++)
    {
        entry0.blocks[1] = i;
        while (__TA_HashTable_increment(hashTable, (__TA_element *)&entry0))
        {
            __TA_resolveClash(hashTable, hashTable->size + 1);
        }
    }

    // now print the resulting hash table to a binary file
    __TA_WriteEdgeHashTable(hashTable, hashTable->getFullSize(hashTable));

    // now make a new hash table and read the output file in
    __TA_HashTable *hashTable2 = (__TA_HashTable *)malloc(sizeof(__TA_HashTable));
    hashTable2->getFullSize = __TA_getFullSize;
    __TA_ReadEdgeHashTable(hashTable2, MARKOV_FILE);
    CheckFileAccuracy(hashTable, hashTable2);

    free(hashTable->array);
    free(hashTable);
    free(hashTable2->array);
    free(hashTable2);
    return 0;
}
