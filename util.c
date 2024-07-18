#include "util.h"
#include "log.h"

// given a raw size, find the nearest power-of-two aligned size (number of bits required to store nBytes)
u8 align_size(size_t nBytes)
{
    u8 powerOfTwo = 0;
    while ((nBytes > (0b1 << powerOfTwo)) > 0)
    {
        powerOfTwo++;
    }
    return powerOfTwo;
}

size_t unalign_size(u8 nBits)
{
    const u8 BITS_IN_BYTE = 8;
    if (nBits >= (sizeof(size_t) * BITS_IN_BYTE))
    {
        InternalError("unalignSize() called with %u bits", nBits);
    }
    return 1 << nBits;
}

ssize_t ssizet_compare(void *dataA, void *dataB)
{
    return (ssize_t)dataA - (ssize_t)dataB;
}

ssize_t sizet_pointer_compare(void *dataA, void *dataB)
{
    // TODO: are you sure about that
    return (*(ssize_t *)dataA) - (*(ssize_t *)dataB);
}

size_t parse_hex_constant(char *hexConstant)
{
    size_t hexValue = 0;
    if ((strncmp(hexConstant, "0x", 2) != 0) && ((strncmp(hexConstant, "0X", 2) != 0)))
    {
        InternalError("Non-hex string %s passed to parseHex", hexConstant);
    }
    for (size_t digitIndex = 2; hexConstant[digitIndex] != '\0'; digitIndex++)
    {
        hexValue <<= 4;

        switch (hexConstant[digitIndex])
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            hexValue += hexConstant[digitIndex] - '0';
            break;

        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            hexValue += hexConstant[digitIndex] - '7';
            break;

        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            hexValue += hexConstant[digitIndex] - 'W';
            break;

        default:
            InternalError("Illegal character %c seen in hex constant", hexConstant[digitIndex]);
            break;
        }
    }

    return hexValue;
}

/*
 *
 *
 */

#include "mbcl/set.h"

// TODO: just use the raw linkedlist data structure in hashtable
ssize_t hash_table_entry_compare(void *dataA, void *dataB)
{
    struct HashTableEntry *entryA = dataA;
    struct HashTableEntry *entryB = dataB;
    return entryA->compareFunction(entryA->key, entryB->key);
}

void hash_table_entry_free(void *entry)
{
    struct HashTableEntry *toFree = entry;
    if (toFree->keyFreeFunction)
    {
        toFree->keyFreeFunction(toFree->key);
    }
    if (toFree->valueFreeFunction)
    {
        toFree->valueFreeFunction(toFree->value);
    }
    free(toFree);
}

struct HashTableEntry *hash_table_entry_new(void *key,
                                            void *value,
                                            ssize_t (*compareFunction)(void *keyA, void *keyB),
                                            void (*keyFreeFunction)(void *key),
                                            void (*valueFreeFunction)(void *value))
{
    struct HashTableEntry *wip = malloc(sizeof(struct HashTableEntry));
    wip->key = key;
    wip->value = value;
    wip->compareFunction = compareFunction;
    wip->keyFreeFunction = keyFreeFunction;
    wip->valueFreeFunction = valueFreeFunction;
    return wip;
}

struct HashTable *hash_table_new(size_t nBuckets,
                                 size_t (*hashFunction)(void *key),
                                 ssize_t (*compareFunction)(void *keyA, void *keyB),
                                 void (*keyFreeFunction)(void *data),
                                 void (*valueFreeFunction)(void *data))
{
    struct HashTable *wip = malloc(sizeof(struct HashTable));
    // TODO: set_free
    array_init(&wip->buckets, (void (*)(void *))set_free, nBuckets);
    wip->hashFunction = hashFunction;
    wip->compareFunction = compareFunction;
    wip->keyFreeFunction = keyFreeFunction;
    wip->valueFreeFunction = valueFreeFunction;

    for (size_t bucketIndex = 0; bucketIndex < nBuckets; bucketIndex++)
    {
        array_emplace(&wip->buckets, bucketIndex, set_new(hash_table_entry_free, hash_table_entry_compare));
    }

    return wip;
}

void *hash_table_lookup(struct HashTable *table, void *key)
{
    size_t hash = table->hashFunction(key);
    hash %= table->buckets.size;

    struct HashTableEntry dummyEntry;
    dummyEntry.key = key;
    dummyEntry.value = NULL;
    dummyEntry.compareFunction = table->compareFunction;

    Set *bucket = array_at(&table->buckets, hash);
    struct HashTableEntry *entryForKey = set_find(bucket, &dummyEntry);
    if (entryForKey == NULL)
    {
        return NULL;
    }
    return entryForKey->value;
}

void hash_table_insert(struct HashTable *table, void *key, void *value)
{
    size_t hash = table->hashFunction(key);
    hash %= table->buckets.size;

    Set *bucket = array_at(&table->buckets, hash);
    struct HashTableEntry *entry = hash_table_entry_new(key, value, table->compareFunction, table->keyFreeFunction, table->valueFreeFunction);
    set_insert(bucket, entry);
}

void hash_table_delete(struct HashTable *table, void *key)
{
    size_t hash = table->hashFunction(key);
    hash %= table->buckets.size;

    struct HashTableEntry dummyEntry;
    dummyEntry.key = key;
    dummyEntry.value = NULL;
    dummyEntry.compareFunction = table->compareFunction;

    Set *bucket = array_at(&table->buckets, hash);
    set_remove(bucket, &dummyEntry);
}

void hash_table_free(struct HashTable *table)
{
    array_deinit(&table->buckets);
    free(table);
}

/*
 * DICTIONARY FUNCTIONS
 * This string hashing algorithm is the djb2 algorithm
 * further information can be found at http://www.cse.yorku.ca/~oz/hash.html
 */
const unsigned int DJB2_HASH_SEED = 5381;
const unsigned int DJB2_MULTIPLCATION_FACTOR = 33;
size_t hash_string(void *data)
{
    char *str = data;
    unsigned int hash = DJB2_HASH_SEED;

    while (*str)
    {
        hash = (hash * DJB2_MULTIPLCATION_FACTOR) + *(str++); /* hash * 33 + c */
    }

    return hash;
}

struct Dictionary *dictionary_new(size_t nBuckets,
                                  void *(*duplicateFunction)(void *),
                                  size_t (*hashFunction)(void *data),
                                  ssize_t (*compareFunction)(void *dataA, void *dataB),
                                  void (*dataFreeFunction)(void *))
{
    struct Dictionary *wip = malloc(sizeof(struct Dictionary));
    wip->table = hash_table_new(nBuckets, hashFunction, (ssize_t(*)(void *, void *))compareFunction, NULL, dataFreeFunction);
    wip->duplicateFunction = duplicateFunction;
    return wip;
}

void *dictionary_insert(struct Dictionary *dict, void *value)
{
    void *duplicatedValue = dict->duplicateFunction(value);
    hash_table_insert(dict->table, duplicatedValue, duplicatedValue);
    return duplicatedValue;
}

void *dictionary_lookup_or_insert(struct Dictionary *dict, void *value)
{
    void *returnedStr = hash_table_lookup(dict->table, value);
    if (returnedStr == NULL)
    {
        returnedStr = dictionary_insert(dict, value);
    }
    return returnedStr;
}

void dictionary_free(struct Dictionary *dict)
{
    hash_table_free(dict->table);
    free(dict);
}

/*
 *
 *
 *
 *
 */

const size_t TEMP_LIST_SPRINTF_LENGTH = 6;
const size_t TEMP_GENERATE_MULTIPLE = 5;
char *temp_list_get(struct TempList *tempList, size_t tempNum)
{
    if (tempNum >= tempList->temps.size)
    {
        size_t generateFrom = tempList->temps.size;
        size_t newMax = tempNum + (TEMP_GENERATE_MULTIPLE - (tempNum % TEMP_GENERATE_MULTIPLE));
        array_resize(&tempList->temps, newMax);
        while (generateFrom < newMax)
        {
            char *thisTemp = malloc(TEMP_LIST_SPRINTF_LENGTH * sizeof(char));
            sprintf(thisTemp, ".t%zu", generateFrom);
            printf("emplace %s at %zu\n", thisTemp, generateFrom);
            array_emplace(&tempList->temps, generateFrom, thisTemp);
            generateFrom++;
        }
    }

    return array_at(&tempList->temps, tempNum);
}

struct TempList *temp_list_new()
{
    struct TempList *wip = malloc(sizeof(struct TempList));
    array_init(&wip->temps, free, 0);
    return wip;
}

void temp_list_free(struct TempList *toFree)
{
    array_deinit(&toFree->temps);
    free(toFree);
}
