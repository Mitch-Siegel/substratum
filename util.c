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
    return *(ssize_t *)dataA - *(ssize_t *)dataB;
}

ssize_t pointer_compare(void *dataA, void *dataB)
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

/*
 * DICTIONARY FUNCTIONS
 * This string hashing algorithm is the djb2 algorithm
 * further information can be found at http://www.cse.yorku.ca/~oz/hash.html
 */

#include "mbcl/hash_table.h"
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

struct Dictionary *dictionary_new(MBCL_DATA_FREE_FUNCTION freeData,
                                  MBCL_DATA_COMPARE_FUNCTION compareKey,
                                  size_t (*hashData)(void *data),
                                  size_t nBuckets,
                                  void *(*duplicateFunction)(void *))
{
    struct Dictionary *wip = malloc(sizeof(struct Dictionary));
    wip->table = hash_table_new(NULL, freeData, compareKey, hashData, nBuckets);
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
    void *returnedStr = hash_table_find(dict->table, value);
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

const size_t TEMP_LIST_SPRINTF_LENGTH = 23;
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

char *sprint_generic_param_names(List *params)
{
    char *str = NULL;
    size_t len = 1;
    Iterator *paramIter = NULL;
    for (paramIter = list_begin(params); iterator_gettable(paramIter); iterator_next(paramIter))
    {
        char *param = iterator_get(paramIter);
        len += strlen(param);
        if (str == NULL)
        {
            str = strdup(param);
        }
        else
        {
            len += 2;
            str = realloc(str, len);
            strlcat(str, ", ", len);
            strlcat(str, param, len);
        }
    }
    iterator_free(paramIter);

    return str;
}
