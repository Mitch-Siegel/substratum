#include "util.h"

// given a raw size, find the nearest power-of-two aligned size (number of bits required to store nBytes)
u8 alignSize(size_t nBytes)
{
    u8 powerOfTwo = 0;
    while ((nBytes > (0b1 << powerOfTwo)) > 0)
    {
        powerOfTwo++;
    }
    return powerOfTwo;
}

size_t unalignSize(u8 nBits)
{
    const u8 bitsInByte = 8;
    if (nBits >= (sizeof(size_t) * bitsInByte))
    {
        ErrorAndExit(ERROR_INTERNAL, "unalignSize() called with %u bits\n", nBits);
    }
    return 1 << nBits;
}

/*
 *
 *
 */

int HashTableEntry_Compare(void *dataA, void *dataB)
{
    struct HashTableEntry *entryA = dataA;
    struct HashTableEntry *entryB = dataB;
    return entryA->compareFunction(entryA->key, entryB->key);
}

struct HashTableEntry *HashTableEntry_New(void *key, void *value, int (*compareFunction)(void *keyA, void *keyB))
{
    struct HashTableEntry *wip = malloc(sizeof(struct HashTableEntry));
    wip->key = key;
    wip->value = value;
    wip->compareFunction = compareFunction;
    return wip;
}

struct HashTable *HashTable_New(size_t nBuckets,
                                size_t (*hashFunction)(void *key),
                                int (*compareFunction)(void *keyA, void *keyB),
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
        wip->buckets[bucketIndex] = Set_New(HashTableEntry_Compare);
    }

    return wip;
}

void *HashTable_Lookup(struct HashTable *table, void *key)
{
    size_t hash = table->hashFunction(key);
    hash %= table->nBuckets;

    struct HashTableEntry dummyEntry;
    dummyEntry.key = key;
    dummyEntry.value = NULL;
    dummyEntry.compareFunction = table->compareFunction;

    struct Set *bucket = table->buckets[hash];
    struct HashTableEntry *entryForKey = Set_Find(bucket, &dummyEntry);
    if (entryForKey == NULL)
    {
        return NULL;
    }
    return entryForKey->value;
}

void HashTable_Insert(struct HashTable *table, void *key, void *value)
{
    size_t hash = table->hashFunction(key);
    hash %= table->nBuckets;

    struct Set *bucket = table->buckets[hash];
    struct HashTableEntry *entry = HashTableEntry_New(key, value, table->compareFunction);
    Set_Insert(bucket, entry);
}

void HashTable_Free(struct HashTable *table)
{
    for (size_t bucketIndex = 0; bucketIndex < table->nBuckets; bucketIndex++)
    {
        struct Set *bucket = table->buckets[bucketIndex];
        for (struct LinkedListNode *entryNode = bucket->elements->head; entryNode != NULL; entryNode = entryNode->next)
        {
            struct HashTableEntry *entry = entryNode->data;
            if (table->keyFreeFunction != NULL)
            {
                table->keyFreeFunction(entry->key);
            }
            if (table->valueFreeFunction != NULL)
            {
                table->valueFreeFunction(entry->value);
            }
            free(entry);
        }
        Set_Free(bucket);
    }
    free(table->buckets);
    free(table);
}

/*
 * DICTIONARY FUNCTIONS
 * This string hashing algorithm is the djb2 algorithm
 * further information can be found at http://www.cse.yorku.ca/~oz/hash.html
 */
const unsigned int djb2HashSeed = 5381;
const unsigned int djb2MultiplcationFactor = 33;
size_t hashString(void *data)
{
    char *str = data;
    unsigned int hash = djb2HashSeed;

    while (*str)
    {
        hash = (hash * djb2MultiplcationFactor) + *(str++); /* hash * 33 + c */
    }

    return hash;
}

struct Dictionary *Dictionary_New(size_t nBuckets)
{
    struct Dictionary *wip = malloc(sizeof(struct Dictionary));
    wip->table = HashTable_New(nBuckets, hashString, (int (*)(void *, void *))strcmp, NULL, free);
    return wip;
}

char *Dictionary_Insert(struct Dictionary *dict, char *value)
{
    char *duplicatedString = strdup(value);
    HashTable_Insert(dict->table, duplicatedString, duplicatedString);
    return duplicatedString;
}

char *Dictionary_LookupOrInsert(struct Dictionary *dict, char *value)
{
    char *returnedStr = HashTable_Lookup(dict->table, value);
    if (returnedStr == NULL)
    {
        returnedStr = Dictionary_Insert(dict, value);
    }
    return returnedStr;
}

void Dictionary_Free(struct Dictionary *dict)
{
    HashTable_Free(dict->table);
    free(dict);
}

/*
 * STACK FUNCTIONS
 *
 */

struct Stack *Stack_New()
{
    struct Stack *wip = malloc(sizeof(struct Stack));
    wip->data = malloc(STACK_DEFAULT_ALLOCATION * sizeof(void *));
    wip->size = 0;
    wip->allocated = STACK_DEFAULT_ALLOCATION;
    return wip;
}

void Stack_Free(struct Stack *stack)
{
    free(stack->data);
    free(stack);
}

void Stack_Push(struct Stack *stack, void *data)
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

void *Stack_Pop(struct Stack *stack)
{
    void *poppedData = NULL;
    if (stack->size > 0)
    {
        poppedData = stack->data[--stack->size];
    }
    else
    {
        printf("Error - attempted to pop from empty stack!\n");
        exit(1);
    }
    return poppedData;
}

void *Stack_Peek(struct Stack *stack)
{
    void *peekedData = NULL;
    if (stack->size > 0)
    {
        peekedData = stack->data[stack->size - 1];
    }
    else
    {
        printf("Error - attempted to peek empty stack!\n");
        exit(1);
    }
    return peekedData;
}

/*
 * LINKED LIST FUNCTIONS
 *
 */

struct LinkedList *LinkedList_New()
{
    struct LinkedList *wip = malloc(sizeof(struct LinkedList));
    wip->head = NULL;
    wip->tail = NULL;
    wip->size = 0;
    return wip;
}

void LinkedList_Free(struct LinkedList *list, void (*dataFreeFunction)(void *))
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

void LinkedList_Append(struct LinkedList *list, void *element)
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

void LinkedList_Prepend(struct LinkedList *list, void *element)
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

void LinkedList_Join(struct LinkedList *before, struct LinkedList *after)
{
    for (struct LinkedListNode *runner = after->head; runner != NULL; runner = runner->next)
    {
        LinkedList_Append(before, runner->data);
    }
}

void *LinkedList_Delete(struct LinkedList *list, int (*compareFunction)(void *, void *), void *element)
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
    ErrorAndExit(ERROR_INTERNAL, "Couldn't delete element from linked list!\n");
}

void *LinkedList_Find(struct LinkedList *list, int (*compareFunction)(void *, void *), void *element)
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

void *LinkedList_PopFront(struct LinkedList *list)
{
    if (list->size == 0)
    {
        ErrorAndExit(ERROR_INVOCATION, "Unable to pop front from empty linkedlist!\n");
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

void *LinkedList_PopBack(struct LinkedList *list)
{
    if (list->size == 0)
    {
        ErrorAndExit(ERROR_INVOCATION, "Unable to pop front from empty linkedlist!\n");
    }
    struct LinkedListNode *popped = list->tail;

    list->tail = list->tail->prev;
    if (list->tail != NULL)
    {
        list->tail->next = NULL;
    }
    list->size--;

    void *poppedData = popped->data;
    free(popped);

    return poppedData;
}

/*
 * Set data structure
 */

struct Set *Set_New(int (*compareFunction)(void *, void *))
{
    struct Set *wip = malloc(sizeof(struct Set));
    wip->elements = LinkedList_New();
    wip->compareFunction = compareFunction;
    return wip;
}

void Set_Insert(struct Set *set, void *element)
{
    if (element == NULL)
    {
        ErrorAndExit(ERROR_INTERNAL, "Attempt to insert null data into set!\n");
    }

    if (LinkedList_Find(set->elements, set->compareFunction, element) == NULL)
    {
        LinkedList_Append(set->elements, element);
    }
}

void Set_Delete(struct Set *set, void *element)
{
    if (LinkedList_Find(set->elements, set->compareFunction, element) != NULL)
    {
        LinkedList_Delete(set->elements, set->compareFunction, element);
    }
    else
    {
        ErrorAndExit(ERROR_INTERNAL, "Attempt to delete non-existent element from set!\n");
    }
}

void *Set_Find(struct Set *set, void *element)
{
    return LinkedList_Find(set->elements, set->compareFunction, element);
}

void Set_Merge(struct Set *into, struct Set *from)
{
    for (struct LinkedListNode *runner = from->elements->head; runner != NULL; runner = runner->next)
    {
        Set_Insert(into, runner->data);
    }
}

struct Set *Set_Copy(struct Set *set)
{
    struct Set *copied = Set_New(set->compareFunction);
    Set_Merge(copied, set);
    return copied;
}

struct Set *Set_Union(struct Set *setA, struct Set *setB)
{
    struct Set *unionedSet = Set_New(setA->compareFunction);
    if (setA->compareFunction != setB->compareFunction)
    {
        ErrorAndExit(ERROR_CODE, "Call to Set_Union with mismatch in set compare functions between sets to union!\n");
    }

    Set_Merge(unionedSet, setA);
    Set_Merge(unionedSet, setB);
    return unionedSet;
}

struct Set *Set_Intersection(struct Set *setA, struct Set *setB)
{
    struct Set *intersectedSet = Set_New(setA->compareFunction);
    if (setA->compareFunction != setB->compareFunction)
    {
        ErrorAndExit(ERROR_CODE, "Call to Set_Union with mismatch in set compare functions between sets to union!\n");
    }

    for (struct LinkedListNode *elementNode = setA->elements->head; elementNode != NULL; elementNode = elementNode->next)
    {
        if (Set_Find(setB, elementNode->data) != NULL)
        {
            Set_Insert(intersectedSet, elementNode->data);
        }
    }

    return intersectedSet;
}

void Set_Free(struct Set *set)
{
    LinkedList_Free(set->elements, NULL);
    free(set);
}

/*
 *
 *
 *
 *
 */

const unsigned int tempListSprintfLength = 6;
char *TempList_Get(struct TempList *tempList, size_t tempNum)
{
    while (tempNum >= tempList->temps->size)
    {
        char *thisTemp = malloc(tempListSprintfLength * sizeof(char));
        sprintf(thisTemp, ".t%zu", tempList->temps->size);
        Stack_Push(tempList->temps, thisTemp);
    }

    return tempList->temps->data[tempNum];
}

struct TempList *TempList_New()
{
    struct TempList *wip = malloc(sizeof(struct TempList));
    wip->temps = Stack_New();
    return wip;
}

void TempList_Free(struct TempList *toFree)
{
    for (size_t tempIndex = 0; tempIndex < toFree->temps->size; tempIndex++)
    {
        free(toFree->temps->data[tempIndex]);
    }
    Stack_Free(toFree->temps);
    free(toFree);
}
