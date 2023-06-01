#include "util.h"

#include <stdlib.h>
#include <unistd.h>

enum LinkedSymbol
{
    s_variable,
    s_function_declaration,
    s_function_definition,
    s_null,
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

struct Type
{
    char isPrimitive;
    union
    {
        int primitive;
        struct
        {
            char isStruct;
            char *name;
        } structOrUnion;
    } type;
};

struct FunctionDeclarationSymbol
{
    char *name;
    struct Type *returnType;
    struct Type *args;
};

struct FunctionDefinitionSymbol
{
    struct FunctionDeclarationSymbol decl;
    struct LinkedList *asmLines;
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
    if (!strcmp(name, "function"))
    {
        return s_function_definition;
    }
    else if (!strcmp(name, "variable"))
    {
        return s_variable;
    }
    else if(!strcmp(name, "section"))
    {
        return s_function_definition;
    }
    else
    {
        ErrorAndExit(ERROR_INTERNAL, "Unexpected symbol name %s\n", name);
    }
}

// returns 0 if export, 1 if require
char parseLinkDirection(char *directionString)
{
    if (!strcmp(directionString, "require"))
    {
        return 1;
    }
    else if (!strcmp(directionString, "export"))
    {
        return 0;
    }
    else
    {
        ErrorAndExit(ERROR_INTERNAL, "Linker Error - unexpected direction %s\n", directionString);
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
            ErrorAndExit(ERROR_INTERNAL, "Error opening file %s\n", (char *)inFileName->data);
        }
        char *inBuf = malloc(513);
        size_t bufSize = 512;

        int len;

        char requireNewSymbol = 1;

        char currentLinkDirection = -1;
        enum LinkedSymbol currentLinkSymbol = s_null;

        while (!feof(inFile))
        {
            len = getline(&inBuf, &bufSize, inFile);
            if (len == -1)
            {
                break;
            }

            if (inBuf[0] == '~')
            {
                // Extract the first token
                char *token = strtok(inBuf + 1, " ");
                if (!strcmp(token, "end"))
                {
                    if (requireNewSymbol)
                    {
                        ErrorAndExit(ERROR_INTERNAL, "Linker error - expected new symbol to start but got end instead!\n");
                    }

                    token = strtok(NULL, " ");
                    if (parseLinkDirection(token) != currentLinkDirection)
                    {
                        ErrorAndExit(ERROR_INTERNAL, "Unexpected end directive - link direction doesn't match start!\n");
                    }

                    token = strtok(NULL, " ");
                    if (symbolNameToEnum(token) != currentLinkSymbol)
                    {
                        ErrorAndExit(ERROR_INTERNAL, "End symbol type doesn't match start!\n");
                    }
                    printf("%s\n", token);
                    requireNewSymbol = 1;
                }
                else
                {
                    if(!requireNewSymbol)
                    {
                        ErrorAndExit(ERROR_INTERNAL, "Started reading new symbol when not expected!\n");
                    }
                    requireNewSymbol = 0;
                    
                    currentLinkDirection = parseLinkDirection(token);
                    currentLinkSymbol = symbolNameToEnum(strtok(NULL, " "));
                }

                // printf("%s\n", token);
            }
            // loop through the string to extract all other tokens
            // while (token != NULL)
            // {
            // printf(" %s\n", token); // printing each token
            // token = strtok(NULL, " ");
            // }
            // printf("\n");
        }
    }
}