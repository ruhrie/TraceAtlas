#include <stdint.h>

// size of the array for each array element
// determines the default maximum number of edges a source node is allowed to have before a collision occurs
#define TUPLE_SIZE 15

// defines how many 4 byte integers we need to hash, as well as other things
#define MARKOV_ORDER 2

// output binary file name
#define MARKOV_FILE "markov.bin"

// always make the size of my structs a power of 2 because this will fit the struct on a single page and cache line
// we need both the source and the sink because we want a hash, not a hash of hashes
typedef struct kvTuple
{
    // source
    uint32_t source;
    // sink
    uint32_t sink;
    // edge count
    uint64_t frequency;
} __TA_kvTuple;

typedef struct arrayElem
{
    // number of neighbors an element can have
    __TA_kvTuple tuple[TUPLE_SIZE];
    // number of active entries in the element
    uint32_t popCount;
    // place holders to align struct to 256 bytes
    uint32_t hold0;
    uint32_t hold1;
    uint32_t hold2;
} __TA_arrayElem;

// we can expand either the arrayElem tuple or the HashTable size to resolve clashes
// we want to keep it shallow (keep arrayElem tuple the same, expand HashTable) because we want constant lookup within the elements
typedef struct HashTable
{
    __TA_arrayElem *array;
    // the array size will always be a power of 2, so this size is the exponent of that power
    uint32_t size;
    uint32_t (*getFullSize)(struct HashTable *self);
} __TA_HashTable;

uint32_t getFullSize(__TA_HashTable *self);

// needs an initializer function, and a function to return the next value and increment this thing
// Motivation
// 1. On a conflict, we need to double the memory so we need to use this to get the next legal tuple out
// 2. Dumping all the data to a file, because the data file is in a different format
// here read is supposed to mean the last valid index I touched (as in, the last index to be read, incremented or written)
typedef struct HashTable_iterator
{
    uint32_t kvIndex;
    __TA_arrayElem *lastRead;
} __TA_HashTable_iterator;

uint32_t __TA_hash(uint32_t x[MARKOV_ORDER]);

uint32_t __TA_hash_source(uint32_t x[MARKOV_ORDER], uint32_t size);

__TA_kvTuple *__TA_tupleLookup(__TA_arrayElem *entry, uint32_t sink);

__TA_arrayElem *__TA_arrayLookup(__TA_HashTable *a, uint32_t source, uint32_t sink);

__TA_kvTuple *__TA_HashTable_read(__TA_HashTable *a, __TA_kvTuple *b);

uint8_t __TA_HashTable_write(__TA_HashTable *a, __TA_kvTuple *b);

uint8_t __TA_HashTable_increment(__TA_HashTable *a, __TA_kvTuple *b);

void __TA_resolveClash();

void __TA_WriteHashTable(__TA_HashTable *a);

void __TA_ReadHashTable(__TA_HashTable *a, char *path);
