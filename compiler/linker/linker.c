#include "util.h"

#include <stdlib.h>
#include <unistd.h>

enum LinkedSymbol
{
    s_variable,
    s_function,
};

enum LinkDirection
{
    export,
    require,
};

struct VariableSymbol
{
    int type;
    int indirectionLevel;
};

struct Symbol
{
    char *name;
    enum LinkDirection direction;
    enum LinkedSymbol symbolType;
    void *data;
};

struct Symbol *Symbol_New(char *name, enum LinkDirection direction, enum LinkedSymbol symbolType, void *data)
{
    struct Symbol *wip = malloc(sizeof(struct Symbol));
    wip->name = name;
    wip->direction = direction;
    wip->symbolType = symbolType;
    wip->data = data;

    return wip;
}

enum LinkedSymbol symbolNameToEnum(char *name)
{
    if(!strcmp(name, "function"))
    {
        return s_function;
    }
    else if(!strcmp(name, "variable"))
    {
        return s_variable;
    }
    else{
        printf("Unexpected symbol name %s\n", name);
        exit(1);
    }
}

int main(int argc, char **argv)
{
    // struct LinkedList *exports = LinkedList_New();
    // struct LinkedList *requires = LinkedList_New();

    struct Dictionary *inputFiles = Dictionary_New(1);

    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "i:")) != -1)
        switch (c)
        {
        case 'i':
            printf("arg -i\n");
            Dictionary_LookupOrInsert(inputFiles, optarg);
            break;

        default:
            printf("something bad happened - aborting!\n");
            abort();
        }

    printf("done with optargs\n");

    if (inputFiles->buckets[0]->size == 0)
    {
        printf("No input files provided!\n");
        exit(1);
    }
    printf("have %d input files\n", inputFiles->buckets[0]->size);

    for (struct LinkedListNode *inFileName = inputFiles->buckets[0]->head; inFileName != NULL; inFileName = inFileName->next)
    {
        printf("opening input file %s\n", (char *)inFileName->data);
        FILE *inFile = fopen(inFileName->data, "rb");
        if (inFile == NULL)
        {
            printf("Error opening file %s\n", (char *)inFileName->data);
            exit(1);
        }
        char *inBuf = malloc(513);
        size_t bufSize = 512;

        int len;



        while (!feof(inFile))
        {
            len = getline(&inBuf, &bufSize, inFile);
            if (len == -1)
            {
                break;
            }

            // Extract the first token
            char *token = strtok(inBuf, " ");
            // loop through the string to extract all other tokens
            while (token != NULL)
            {
                printf(" %s\n", token); // printing each token
                token = strtok(NULL, " ");
            }
            printf("\n");
        }
    }
}