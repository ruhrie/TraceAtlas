#include "Backend/DashHashTable.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define HASH_MULTIPLIER 1000003UL
#define HASH_MULTIPLIER_OFFSET 82520UL
#define HASH_INITIAL 0x12345678UL
#define HASH_OFFSET 97531UL

#define INTERNAL_CLASHES_LIMIT 5

#ifdef __cplusplus
extern "C"
{
#endif
    // used for expanding the size exponent of a HashTable struct
    uint32_t __TA_getFullSize(__TA_HashTable *self)
    {
        return 0x1 << self->size;
    }

    // used for hashing an integer
    uint32_t __TA_hash(uint32_t x[MARKOV_ORDER + 1])
    {
        uint32_t m = HASH_MULTIPLIER;
        uint32_t y = HASH_INITIAL;
        for (int i = 0; i < MARKOV_ORDER + 1; i++)
        {
            y += (x[i] >> 16) ^ x[i] * m + HASH_OFFSET;
            m += HASH_MULTIPLIER_OFFSET + MARKOV_ORDER + MARKOV_ORDER;
        }
        return y;
    };

    // here size is ceil( log2(arraySize) )
    uint32_t __TA_hash_source(uint32_t x[MARKOV_ORDER + 1], uint32_t size)
    {
        // instead of doing a mask, I should do a right shift
        // so instead of modulo, we want to do a right shift by (32-arraysize)
        // the mask just needs to be 0x(size - 1)
        uint32_t shortHash = 0;
        uint32_t longHash = __TA_hash(x);
        uint32_t mask = 0x1;
        for (uint32_t i = 0; i < size; i++)
        {
            shortHash += longHash & mask;
            mask = mask << 1;
        }
        return shortHash;
    }

    __TA_element *__TA_tupleLookup(__TA_arrayElem *entry, __TA_element *index)
    {
        for (uint32_t i = 0; i < entry->popCount; i++)
        {
            for (uint32_t j = 0; j < MARKOV_ORDER + 1; j++)
            {
                if ((entry->tuple[i].edge.blocks[j] == index->edge.blocks[j]) && (entry->tuple[i].edge.blocks[j] == index->edge.blocks[j]))
                {
                    return &entry->tuple[i];
                }
            }
        }
        return NULL;
    }

    __TA_arrayElem *__TA_arrayLookup(__TA_HashTable *a, __TA_element *index)
    {
        return &a->array[__TA_hash_source(index->edge.blocks, a->size)];
    }

    __TA_element *__TA_HashTable_read(__TA_HashTable *a, __TA_element *b)
    {
        __TA_arrayElem *entry = __TA_arrayLookup(a, b);
        return __TA_tupleLookup(entry, b);
    }

    uint8_t __TA_HashTable_write(__TA_HashTable *a, __TA_element *b)
    {
        __TA_arrayElem *index = __TA_arrayLookup(a, b);
        __TA_element *entry = __TA_tupleLookup(index, b);
        if (entry)
        {
            *entry = *b;
        }
        else
        {
            // check to see if we have a collision, and if we do return error message
            if (index->popCount == TUPLE_SIZE)
            {
                return 1;
            }
            // we just have to make a new entry
            else
            {
                index->tuple[index->popCount] = *b;
                index->popCount++;
            }
        }
        return 0;
    }

    uint8_t __TA_HashTable_increment(__TA_HashTable *a, __TA_element *b)
    {
        __TA_arrayElem *index = __TA_arrayLookup(a, b);
        __TA_element *entry = __TA_tupleLookup(index, b);
        if (entry)
        {
            (entry->edge.frequency)++;
        }
        else
        {
            // check to see if we have a collision, and if we do return error message
            if (index->popCount == TUPLE_SIZE)
            {
                return 1;
            }
            // we just have to make a new entry
            else
            {
                index->tuple[index->popCount] = *b;
                index->tuple[index->popCount].edge.frequency++;
                index->popCount++;
            }
        }
        return 0;
    }

    void __TA_resolveClash(__TA_HashTable *hashTable, uint32_t newSize)
    {
        static bool freedOld = false;
        // counts the number of recursions we do. If it exceeds INTERNAL_CLASHES_LIMIT we quit and exit the program.
        static uint8_t internalClashes = 0;
        printf("We have had %d recursions to resolve internal clashes.\n", internalClashes);
        if (internalClashes == INTERNAL_CLASHES_LIMIT)
        {
            printf("We have exceeded the number of internal clashes allowed. Exiting...");
            return;
        }
        // local copy of the old hashTable
        __TA_HashTable old;
        old.size = hashTable->size;
        old.array = hashTable->array;
        old.getFullSize = hashTable->getFullSize;
        // we double the size of the array each time there is a clash
        hashTable->size = newSize;
        // reallocate a new array that has double the current entries
        hashTable->array = (__TA_arrayElem *)malloc(hashTable->getFullSize(hashTable) * sizeof(__TA_arrayElem));
        if (!hashTable->array)
        {
            printf("Malloc failed!");
        }
        // put in everything from the old array
        printf("Old size is %d and new size is %d\n", old.getFullSize(&old), hashTable->getFullSize(hashTable));
        bool recursed = false;
        for (uint32_t i = 0; i < old.getFullSize(&old); i++)
        {
            for (uint32_t j = 0; j < old.array[i].popCount; j++)
            {
                while (__TA_HashTable_write(hashTable, &old.array[i].tuple[j]))
                {
                    // john: this should be impossible. When we double the size on an entry that clashes we can't have a conflict with copied, only the new entry.
                    // john: always complete the copy before we start a new recursion
                    // john: this loop will segfault as soon as the recursion returns
                    // john: start with small default hash sizes in TBs
                    /*internalClashes++;
                    // we need to preserve the old array while incrementing the size
                    free(hashTable->array);
                    hashTable->array = old.array;
                    hashTable->size = old.size;
                    printf("Recursing into resolveClash!\n");
                    __TA_resolveClash(hashTable, hashTable->size + (uint32_t)internalClashes);
                    internalClashes--;
                    printf("After decrement, internalClashes is now %d\n", internalClashes);
                    recursed = true;
                    break;*/
                }
            }
            if (recursed)
            {
                break;
            }
        }
        printf("Finished initializing new array\n");
        // should only happen on the bottom of the recursion depth
        if (!freedOld)
        {
            //free(old.array);
            freedOld = true;
            printf("Resolved the clash!\n");
        }
        // should only happen on the top call to __TA_resolveClash (top of the recursion depth)
        if (!internalClashes)
        {
            freedOld = false;
        }
    }

    // thus function is only designed to use edgeTuple objects
    void __TA_WriteEdgeHashTable(__TA_HashTable *a, uint32_t blockCount)
    {
        char *p = getenv("MARKOV_FILE");
        FILE *f;
        if (p)
        {
            f = fopen(p, "wb");
        }
        else
        {
            f = fopen(MARKOV_FILE, "wb");
        }
        // first write the markov order of the graph
        uint32_t MO = MARKOV_ORDER;
        fwrite(&MO, sizeof(uint32_t), 1, f);
        // second write the total number of blocks in the graph (each block may or may not be connected to the rest of the graph)
        fwrite(&blockCount, sizeof(uint32_t), 1, f);
        // third write the number of edges in the file
        uint32_t edges = 0;
        uint32_t liveArrayEntries = 0;
        uint32_t maxPopCount = 0;
        for (uint32_t i = 0; i < a->getFullSize(a); i++)
        {
            if (a->array[i].popCount)
            {
                liveArrayEntries++;
                if (a->array[i].popCount > maxPopCount)
                {
                    maxPopCount = a->array[i].popCount;
                }
            }
            edges += a->array[i].popCount;
        }
        fwrite(&edges, sizeof(uint32_t), 1, f);

        // third, write all the edges
        for (uint32_t i = 0; i < a->getFullSize(a); i++)
        {
            for (uint32_t j = 0; j < a->array[i].popCount; j++)
            {
                fwrite(&a->array[i].tuple[j], sizeof(__TA_element), 1, f);
            }
        }
        fclose(f);
        // calculate some statistics about our hash table
        // number of nodes
        printf("\nHASHTABLESIZE: %d\n", blockCount);
        // number of edges
        printf("\nHASHTABLEEDGES: %d\n", edges);
        // live array entries
        printf("\nHASHTABLELIVEARRAYENTRIES: %d\n", liveArrayEntries);
        // maximum occupancy of an array element
        printf("\nHASHTABLEMAXPOPCOUNT: %d\n", maxPopCount);
    }

    void __TA_ReadEdgeHashTable(__TA_HashTable *a, char *path)
    {
        FILE *f = fopen(path, "rb");
        // first word is a uint32_t of the markov order of the graph
        uint32_t markovOrder;
        fread(&markovOrder, sizeof(uint32_t), 1, f);
        // second word is the number of nodes in the graph
        uint32_t blocks;
        fread(&blocks, sizeof(uint32_t), 1, f);
        // third word is a uint32_t of how many edges there are in the file
        uint32_t edges;
        fread(&edges, sizeof(uint32_t), 1, f);

        // estimate that each node has 2 neighbors on average, thus allocate edge/2 nodes in the hash table
        a->size = (uint32_t)(ceil(log((double)edges / 2.0) / log(2.0)));
        a->array = (__TA_arrayElem *)malloc(a->getFullSize(a) * sizeof(__TA_arrayElem));

        // read all the edges
        // for now, only works when MARKOV_ORDER=1
        __TA_element newEntry;
        // the __TA_element union contains 2 additional words for blockLabel char*
        uint64_t garbage = 0;
        for (uint32_t i = 0; i < edges; i++)
        {
            fread(&newEntry.edge.blocks[0], sizeof(uint32_t), 1, f);
            fread(&newEntry.edge.blocks[1], sizeof(uint32_t), 1, f);
            fread(&newEntry.edge.frequency, sizeof(uint64_t), 1, f);
            fread(&garbage, sizeof(uint64_t), 1, f);
            while (__TA_HashTable_write(a, &newEntry))
            {
                __TA_resolveClash(a, a->size + 1);
            }
        }
        fclose(f);
    }

#ifdef __cplusplus
}
#endif