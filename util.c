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
    wip->nBuckets = nBuckets;
    wip->buckets = malloc(nBuckets * sizeof(struct Set *));
    wip->hashFunction = hashFunction;
    wip->compareFunction = compareFunction;
    wip->keyFreeFunction = keyFreeFunction;
    wip->valueFreeFunction = valueFreeFunction;

    for (size_t bucketIndex = 0; bucketIndex < nBuckets; bucketIndex++)
    {
        wip->buckets[bucketIndex] = set_new(hash_table_entry_compare, hash_table_entry_free);
    }

    return wip;
}

void *hash_table_lookup(struct HashTable *table, void *key)
{
    size_t hash = table->hashFunction(key);
    hash %= table->nBuckets;

    struct HashTableEntry dummyEntry;
    dummyEntry.key = key;
    dummyEntry.value = NULL;
    dummyEntry.compareFunction = table->compareFunction;

    struct Set *bucket = table->buckets[hash];
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
    hash %= table->nBuckets;

    struct Set *bucket = table->buckets[hash];
    struct HashTableEntry *entry = hash_table_entry_new(key, value, table->compareFunction, table->keyFreeFunction, table->valueFreeFunction);
    set_insert(bucket, entry);
}

void hash_table_delete(struct HashTable *table, void *key)
{
    size_t hash = table->hashFunction(key);
    hash %= table->nBuckets;

    struct HashTableEntry dummyEntry;
    dummyEntry.key = key;
    dummyEntry.value = NULL;
    dummyEntry.compareFunction = table->compareFunction;

    struct Set *bucket = table->buckets[hash];
    set_delete(bucket, &dummyEntry);
}

void hash_table_free(struct HashTable *table)
{
    for (size_t bucketIndex = 0; bucketIndex < table->nBuckets; bucketIndex++)
    {
        struct Set *bucket = table->buckets[bucketIndex];
        set_free(bucket);
    }
    free(table->buckets);
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
 * STACK FUNCTIONS
 *
 */

struct Stack *old_stack_new()
{
    struct Stack *wip = malloc(sizeof(struct Stack));
    wip->data = malloc(STACK_DEFAULT_ALLOCATION * sizeof(void *));
    wip->size = 0;
    wip->allocated = STACK_DEFAULT_ALLOCATION;
    return wip;
}

void old_stack_free(struct Stack *stack)
{
    free(stack->data);
    free(stack);
}

void old_stack_push(struct Stack *stack, void *data)
{
    if (stack->size >= stack->allocated)
    {
        void **newData = malloc((stack->allocated * STACK_SCALE_FACTOR) * sizeof(void *));
        memcpy(newData, stack->data, stack->allocated * sizeof(void *));
        free(stack->data);
        stack->data = newData;
        stack->allocated = (stack->allocated * STACK_SCALE_FACTOR);
    }

    stack->data[stack->size++] = data;
}

void *old_stack_pop(struct Stack *stack)
{
    void *poppedData = NULL;
    if (stack->size > 0)
    {
        poppedData = stack->data[--stack->size];
    }
    else
    {
        log(LOG_FATAL, "Error - attempted to pop from empty stack!");
    }
    return poppedData;
}

void *old_stack_peek(struct Stack *stack)
{
    void *peekedData = NULL;
    if (stack->size > 0)
    {
        peekedData = stack->data[stack->size - 1];
    }
    else
    {
        log(LOG_FATAL, "Error - attempted to peek empty stack!");
    }
    return peekedData;
}

/*
 * LINKED LIST FUNCTIONS
 *
 */

struct LinkedList *linked_list_new()
{
    struct LinkedList *wip = malloc(sizeof(struct LinkedList));
    wip->head = NULL;
    wip->tail = NULL;
    wip->size = 0;
    return wip;
}

void linked_list_free(struct LinkedList *list, void (*dataFreeFunction)(void *))
{
    struct LinkedListNode *runner = list->head;
    while (runner != NULL)
    {
        if (dataFreeFunction != NULL)
        {
            dataFreeFunction(runner->data);
        }
        struct LinkedListNode *old = runner;
        runner = runner->next;
        free(old);
    }
    free(list);
}

void linked_list_append(struct LinkedList *list, void *element)
{
    if (element == NULL)
    {
        perror("Attempt to append data with null pointer into LinkedList!");
        exit(1);
    }

    struct LinkedListNode *newNode = malloc(sizeof(struct LinkedListNode));
    newNode->data = element;
    if (list->size == 0)
    {
        newNode->next = NULL;
        newNode->prev = NULL;
        list->head = newNode;
        list->tail = newNode;
    }
    else
    {
        list->tail->next = newNode;
        newNode->prev = list->tail;
        newNode->next = NULL;
        list->tail = newNode;
    }
    list->size++;
}

void linked_list_prepend(struct LinkedList *list, void *element)
{
    if (element == NULL)
    {
        perror("Attempt to prepend data with null pointer into LinkedList!");
        exit(1);
    }

    struct LinkedListNode *newNode = malloc(sizeof(struct LinkedListNode));
    newNode->data = element;
    if (list->size == 0)
    {
        newNode->next = NULL;
        newNode->prev = NULL;
        list->head = newNode;
        list->tail = newNode;
    }
    else
    {
        list->head->prev = newNode;
        newNode->next = list->head;
        list->head = newNode;
    }
    list->size++;
}

void linked_list_join(struct LinkedList *before, struct LinkedList *after)
{
    for (struct LinkedListNode *runner = after->head; runner != NULL; runner = runner->next)
    {
        linked_list_append(before, runner->data);
    }
}

void *linked_list_delete(struct LinkedList *list, ssize_t (*compareFunction)(void *, void *), void *element)
{
    for (struct LinkedListNode *runner = list->head; runner != NULL; runner = runner->next)
    {
        if (!compareFunction(runner->data, element))
        {
            if (list->size > 1)
            {
                if (runner == list->head)
                {
                    list->head = runner->next;
                    runner->next->prev = NULL;
                }
                else
                {
                    if (runner == list->tail)
                    {
                        list->tail = runner->prev;
                        runner->prev->next = NULL;
                    }
                    else
                    {
                        runner->prev->next = runner->next;
                        runner->next->prev = runner->prev;
                    }
                }
            }
            else
            {
                list->head = NULL;
                list->tail = NULL;
            }
            void *data = runner->data;
            free(runner);
            list->size--;
            return data;
        }
    }
    InternalError("Couldn't delete element from linked list!");
}

void *linked_list_find(struct LinkedList *list, ssize_t (*compareFunction)(void *, void *), void *element)
{
    for (struct LinkedListNode *runner = list->head; runner != NULL; runner = runner->next)
    {
        if (!compareFunction(runner->data, element))
        {
            return runner->data;
        }
    }
    return NULL;
}

void *linked_list_pop_front(struct LinkedList *list)
{
    if (list->size == 0)
    {
        log(LOG_FATAL, "Unable to pop front from empty linkedlist!");
    }
    struct LinkedListNode *popped = list->head;

    list->head = list->head->next;
    if (list->head != NULL)
    {
        list->head->prev = NULL;
    }
    else
    {
        list->tail = NULL;
    }
    list->size--;

    void *poppedData = popped->data;
    free(popped);

    return poppedData;
}

void *linked_list_pop_back(struct LinkedList *list)
{
    if (list->size == 0)
    {
        log(LOG_FATAL, "Unable to pop front from empty linkedlist!");
    }
    struct LinkedListNode *popped = list->tail;

    list->size--;

    if (list->size)
    {
        list->tail = list->tail->prev;
    }
    else
    {
        list->tail = NULL;
        ;
        list->head = NULL;
    }

    void *poppedData = popped->data;
    free(popped);

    return poppedData;
}

/*
 * Set data structure
 */

struct Set *set_new(ssize_t (*compareFunction)(void *elementA, void *elementB), void(*dataFreeFunction))
{
    struct Set *wip = malloc(sizeof(struct Set));
    wip->elements = linked_list_new();
    wip->compareFunction = compareFunction;
    wip->dataFreeFunction = dataFreeFunction;
    return wip;
}

void set_insert(struct Set *set, void *element)
{
    if (element == NULL)
    {
        InternalError("Attempt to insert null data into set!");
    }

    if (linked_list_find(set->elements, set->compareFunction, element) == NULL)
    {
        linked_list_append(set->elements, element);
    }
}

void set_delete(struct Set *set, void *element)
{
    if (linked_list_find(set->elements, set->compareFunction, element) != NULL)
    {
        void *dataToFree = linked_list_delete(set->elements, set->compareFunction, element);
        if (set->dataFreeFunction != NULL)
        {
            set->dataFreeFunction(dataToFree);
        }
    }
    else
    {
        InternalError("Attempt to delete non-existent element from set!");
    }
}

void *set_find(struct Set *set, void *element)
{
    return linked_list_find(set->elements, set->compareFunction, element);
}

void set_clear(struct Set *toClear)
{
    linked_list_free(toClear->elements, toClear->dataFreeFunction);
    toClear->elements = linked_list_new();
}

void set_merge(struct Set *into, struct Set *from)
{
    for (struct LinkedListNode *runner = from->elements->head; runner != NULL; runner = runner->next)
    {
        set_insert(into, runner->data);
    }
}

struct Set *set_copy(struct Set *set)
{
    struct Set *copied = set_new(set->compareFunction, set->dataFreeFunction);
    set_merge(copied, set);
    return copied;
}

struct Set *set_union(struct Set *setA, struct Set *setB)
{
    struct Set *unionedSet = set_new(setA->compareFunction, setA->dataFreeFunction);
    if (setA->compareFunction != setB->compareFunction)
    {
        log(LOG_FATAL, "Call to SetUnion with mismatch in set compare functions between sets to union!");
    }
    if (setA->dataFreeFunction != setB->dataFreeFunction)
    {
        log(LOG_FATAL, "Call to SetUnion with mismatch in set data free functions between sets to union!");
    }

    set_merge(unionedSet, setA);
    set_merge(unionedSet, setB);
    return unionedSet;
}

struct Set *set_intersection(struct Set *setA, struct Set *setB)
{
    struct Set *intersectedSet = set_new(setA->compareFunction, setA->dataFreeFunction);
    if (setA->compareFunction != setB->compareFunction)
    {
        log(LOG_FATAL, "Call to SetIntersection with mismatch in set compare functions between sets to intersect!");
    }
    if (setA->dataFreeFunction != setB->dataFreeFunction)
    {
        log(LOG_FATAL, "Call to SetIntersection with mismatch in set data free functions between sets to intersect!");
    }

    for (struct LinkedListNode *elementNode = setA->elements->head; elementNode != NULL; elementNode = elementNode->next)
    {
        if (set_find(setB, elementNode->data) != NULL)
        {
            set_insert(intersectedSet, elementNode->data);
        }
    }

    return intersectedSet;
}

void set_free(struct Set *set)
{
    linked_list_free(set->elements, set->dataFreeFunction);
    free(set);
}

/*
 *
 *
 *
 *
 */

const unsigned int TEMP_LIST_SPRINTF_LENGTH = 6;
char *temp_list_get(struct TempList *tempList, size_t tempNum)
{
    while (tempNum >= tempList->temps->size)
    {
        char *thisTemp = malloc(TEMP_LIST_SPRINTF_LENGTH * sizeof(char));
        sprintf(thisTemp, ".t%zu", tempList->temps->size);
        old_stack_push(tempList->temps, thisTemp);
    }

    return tempList->temps->data[tempNum];
}

struct TempList *temp_list_new()
{
    struct TempList *wip = malloc(sizeof(struct TempList));
    wip->temps = old_stack_new();
    return wip;
}

void temp_list_free(struct TempList *toFree)
{
    for (size_t tempIndex = 0; tempIndex < toFree->temps->size; tempIndex++)
    {
        free(toFree->temps->data[tempIndex]);
    }
    old_stack_free(toFree->temps);
    free(toFree);
}
