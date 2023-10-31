#include "util.h"


// given a raw size, find the nearest power-of-two aligned size
int alignSize(int nBytes)
{
	int i = 0;
	while ((nBytes > (0b1 << i)) > 0)
	{
		i++;
	}
	return i;
}

/*
 * DICTIONARY FUNCTIONS
 * This string hashing algorithm is the djb2 algorithm
 * further information can be found at http://www.cse.yorku.ca/~oz/hash.html
 */
unsigned int hash(char *str)
{
	unsigned int hash = 5381;
	unsigned int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

struct Dictionary *Dictionary_New(int nBuckets)
{
	struct Dictionary *wip = malloc(sizeof(struct Dictionary));
	wip->nBuckets = nBuckets;
	wip->buckets = malloc(nBuckets * sizeof(struct LinkedList *));

	for (int i = 0; i < nBuckets; i++)
	{
		wip->buckets[i] = LinkedList_New();
	}

	return wip;
}

char *Dictionary_Insert(struct Dictionary *dict, char *value)
{

	char *string = malloc(strlen(value) + 1);
	strncpy(string, value, strlen(value));
	string[strlen(value)] = '\0';
	unsigned int strHash = hash(value);
	strHash = strHash % dict->nBuckets;

	LinkedList_Append(dict->buckets[strHash], string);

	return string;
}

char *Dictionary_Lookup(struct Dictionary *dict, char *value)
{
	unsigned int strHash = hash(value);
	strHash = strHash % dict->nBuckets;

	struct LinkedList *bucket = dict->buckets[strHash];
	if (bucket->size == 0)
	{
		return NULL;
	}

	struct LinkedListNode *runner = bucket->head;

	while (runner != NULL)
	{
		if (!strcmp(runner->data, value))
		{
			return runner->data;
		}

		runner = runner->next;
	}
	return NULL;
}

char *Dictionary_LookupOrInsert(struct Dictionary *dict, char *value)
{
	char *lookupResult = Dictionary_Lookup(dict, value);
	if (lookupResult != NULL)
	{
		return lookupResult;
	}
	else
	{
		char *pointer = Dictionary_Insert(dict, value);
		return pointer;
	}
}

void Dictionary_Free(struct Dictionary *dict)
{
	for (int i = 0; i < dict->nBuckets; i++)
	{
		LinkedList_Free(dict->buckets[i], free);
	}
	free(dict->buckets);
	free(dict);
}

/*
 * STACK FUNCTIONS
 *
 */

struct Stack *Stack_New()
{
	struct Stack *wip = malloc(sizeof(struct Stack));
	wip->data = malloc(20 * sizeof(void *));
	wip->size = 0;
	wip->allocated = 20;
	return wip;
}

void Stack_Free(struct Stack *s)
{
	free(s->data);
	free(s);
}

void Stack_Push(struct Stack *s, void *data)
{
	if (s->size >= s->allocated)
	{
		void **newData = malloc((int)(s->allocated * 1.5) * sizeof(void *));
		memcpy(newData, s->data, s->allocated * sizeof(void *));
		free(s->data);
		s->data = newData;
		s->allocated = (int)(s->allocated * 1.5);
	}

	s->data[s->size++] = data;
}

void *Stack_Pop(struct Stack *s)
{
	if (s->size > 0)
	{
		return s->data[--s->size];
	}
	else
	{
		printf("Error - attempted to pop from empty stack!\n");
		exit(1);
	}
}

void *Stack_Peek(struct Stack *s)
{
	if (s->size > 0)
	{
		return s->data[s->size - 1];
	}
	else
	{
		printf("Error - attempted to peek empty stack!\n");
		exit(1);
	}
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

void LinkedList_Free(struct LinkedList *l, void (*dataFreeFunction)())
{
	struct LinkedListNode *runner = l->head;
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
	free(l);
}

void LinkedList_Append(struct LinkedList *l, void *element)
{
	if (element == NULL)
	{
		perror("Attempt to append data with null pointer into LinkedList!");
		exit(1);
	}

	struct LinkedListNode *newNode = malloc(sizeof(struct LinkedListNode));
	newNode->data = element;
	if (l->size == 0)
	{
		newNode->next = NULL;
		newNode->prev = NULL;
		l->head = newNode;
		l->tail = newNode;
	}
	else
	{
		l->tail->next = newNode;
		newNode->prev = l->tail;
		newNode->next = NULL;
		l->tail = newNode;
	}
	l->size++;
}

void LinkedList_Prepend(struct LinkedList *l, void *element)
{
	if (element == NULL)
	{
		perror("Attempt to prepend data with null pointer into LinkedList!");
		exit(1);
	}

	struct LinkedListNode *newNode = malloc(sizeof(struct LinkedListNode));
	newNode->data = element;
	if (l->size == 0)
	{
		newNode->next = NULL;
		newNode->prev = NULL;
		l->head = newNode;
		l->tail = newNode;
	}
	else
	{
		l->head->prev = newNode;
		newNode->next = l->head;
		l->head = newNode;
	}
	l->size++;
}

void LinkedList_Join(struct LinkedList *before, struct LinkedList *after)
{
	for(struct LinkedListNode *runner = after->head; runner != NULL; runner = runner->next)
	{
		LinkedList_Append(before, runner->data);
	}
}

void *LinkedList_Delete(struct LinkedList *l, int (*compareFunction)(), void *element)
{
	for (struct LinkedListNode *runner = l->head; runner != NULL; runner = runner->next)
	{
		if (!compareFunction(runner->data, element))
		{
			if (l->size > 1)
			{
				if (runner == l->head)
				{
					l->head = runner->next;
					runner->next->prev = NULL;
				}
				else
				{
					if (runner == l->tail)
					{
						l->tail = runner->prev;
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
				l->head = NULL;
				l->tail = NULL;
			}
			void *data = runner->data;
			free(runner);
			l->size--;
			return data;
		}
	}
	ErrorAndExit(ERROR_INTERNAL, "Couldn't delete element from linked list!\n");
}

void *LinkedList_Find(struct LinkedList *l, int (*compareFunction)(), void *element)
{
	for (struct LinkedListNode *runner = l->head; runner != NULL; runner = runner->next)
	{
		if (!compareFunction(runner->data, element))
		{
			return runner->data;
		}
	}
	return NULL;
}

void *LinkedList_PopFront(struct LinkedList *l)
{
	if (l->size == 0)
	{
		ErrorAndExit(ERROR_INVOCATION, "Unable to pop front from empty linkedlist!\n");
	}
	struct LinkedListNode *popped = l->head;

	l->head = l->head->next;
	l->head->prev = NULL;
	l->size--;

	void *poppedData = popped->data;
	free(popped);

	return poppedData;
}

void *LinkedList_PopBack(struct LinkedList *l)
{
	if (l->size == 0)
	{
		ErrorAndExit(ERROR_INVOCATION, "Unable to pop front from empty linkedlist!\n");
	}
	struct LinkedListNode *popped = l->tail;

	l->tail = l->tail->prev;
	l->tail->next = NULL;
	l->size--;

	void *poppedData = popped->data;
	free(popped);

	return poppedData;
}

/*
 * string trimming - overallocate for sprintf and let this funciton trim it down
 */

char *strTrim(char *s, int l)
{
	char *newStr = malloc(l + 1);
	strcpy(newStr, s);
	return newStr;
}

char *strAppend(char *before, char *after)
{
	int beforeLen = strlen(before);
	int afterLen = strlen(after);

	char *newStr = malloc(beforeLen + afterLen + 1);
	strcpy(newStr, before);
	strcpy(newStr + beforeLen, after);

	free(before);
	free(after);
	return newStr;
}

/*
 *
 *
 *
 *
 */

char *TempList_Get(struct TempList *tempList, int tempNum)
{
	int sizeDiff = tempNum - tempList->temps->size;
	while (sizeDiff-- >= 0)
	{
		char *thisTemp = malloc(6 * sizeof(char));
		sprintf(thisTemp, ".t%d", tempList->temps->size);
		Stack_Push(tempList->temps, thisTemp);
	}
	return tempList->temps->data[tempNum];
}

struct TempList *TempList_New()
{
	struct TempList *wip = malloc(sizeof(struct TempList));
	wip->temps = Stack_New();
	TempList_Get(wip, 10);
	return wip;
}

void TempList_Free(struct TempList *it)
{
	for (int i = 0; i < it->temps->size; i++)
	{
		free(it->temps->data[i]);
	}
	Stack_Free(it->temps);
	free(it);
}
