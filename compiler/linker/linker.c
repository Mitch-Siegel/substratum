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

struct Symbol
{
    char *name;                   // string name of the symbol
    enum LinkDirection direction; // whether this symbol is exported or required from this file
    enum LinkedSymbol symbolType; // what type of symbol this is
    union
    {
        struct VariableSymbol asVariable;
        struct FunctionDeclarationSymbol asFunction;
    } data;                  // union exact details about this symbol
    struct LinkedList *lines; // raw data of any text lines containing asm
};

struct Symbol *Symbol_New(char *name, enum LinkDirection direction, enum LinkedSymbol symbolType)
{
    struct Symbol *wip = malloc(sizeof(struct Symbol));
    wip->name = name;
    wip->direction = direction;
    wip->symbolType = symbolType;
    wip->lines = LinkedList_New();

    return wip;
}

void Symbol_Free(struct Symbol *s)
{
    LinkedList_Free(s->lines, free);
    free(s);
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
    else if (!strcmp(name, "section"))
    {
        return s_function_definition;
    }
    else
    {
        ErrorAndExit(ERROR_INTERNAL, "Unexpected symbol name %s\n", name);
    }
}

char *symbolEnumToName(enum LinkedSymbol s)
{
    switch(s)
    {
        case s_function_declaration:
            return "function declaration";

        case s_function_definition:
            return "function definition";

        case s_variable:
            return "variable";

        case s_null:
            return "s_null";
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

// compare symbols by type and string name
int compareSymbols(struct Symbol *a, struct Symbol *b)
{
    if (a->symbolType != b->symbolType)
    {
        return 1;
    }
    return strcmp(a->name, b->name);
}

void addExport(struct LinkedList **exports, struct LinkedList **requires, struct Symbol *toAdd)
{
    enum LinkedSymbol symbolType = toAdd->symbolType;
    if (LinkedList_Find(requires[symbolType], compareSymbols, toAdd))
    {
        struct Symbol *deletedRequire = LinkedList_Delete(requires[symbolType], compareSymbols, toAdd);
        Symbol_Free(deletedRequire);
    }
    LinkedList_Append(exports[symbolType], toAdd);
}

void addRequire(struct LinkedList **exports, struct LinkedList **requires, struct Symbol *toRequire)
{
    enum LinkedSymbol symbolType = toRequire->symbolType;
    if (!LinkedList_Find(exports[symbolType], compareSymbols, toRequire))
    {
        LinkedList_Append(requires[symbolType], toRequire);
    }
}

int main(int argc, char **argv)
{
    struct LinkedList *exports[s_null];
    struct LinkedList *
        requires[s_null];

    for (int i = 0; i < s_null; i++)
    {
        exports[i] = LinkedList_New();
        requires[i] = LinkedList_New();
    }

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
        enum LinkedSymbol currentLinkSymbolType = s_null;
        struct Symbol *currentSymbol = NULL;

        while (!feof(inFile))
        {
            len = getline(&inBuf, &bufSize, inFile);
            if (len == -1)
            {
                break;
            }
            inBuf[len - 1] = '\0';
            len--;

            if (inBuf[0] == '~')
            {
                // Extract the first token
                char *token = strtok(inBuf + 1, " ");
                if (!strcmp(token, "end"))
                {
                    printf("end ");
                    if (requireNewSymbol)
                    {
                        ErrorAndExit(ERROR_INTERNAL, "Linker error - expected new symbol to start but got end instead!\n");
                    }

                    token = strtok(NULL, " ");
                    printf("%s ", token);
                    if (parseLinkDirection(token) != currentLinkDirection)
                    {
                        ErrorAndExit(ERROR_INTERNAL, "Unexpected end directive - link direction doesn't match start!\n");
                    }

                    token = strtok(NULL, " ");
                    printf("%s ", token);
                    if (symbolNameToEnum(token) != currentLinkSymbolType)
                    {
                        ErrorAndExit(ERROR_INTERNAL, "End symbol type doesn't match start!\n");
                    }

                    token = strtok(NULL, " ");
                    printf("%s\n\n", token);
                    if (strcmp(token, currentSymbol->name))
                    {
                        ErrorAndExit(ERROR_INTERNAL, "End symbol name (%s) doesn't match start (%s)!\n", token, currentSymbol->name);
                    }

                    requireNewSymbol = 1;
                }
                else
                {
                    if (!requireNewSymbol)
                    {
                        ErrorAndExit(ERROR_INTERNAL, "Started reading new symbol when not expected!\n");
                    }
                    requireNewSymbol = 0;

                    currentLinkDirection = parseLinkDirection(token);
                    printf("%s ", token);

                    token = strtok(NULL, " ");
                    printf("%s ", token);
                    currentLinkSymbolType = symbolNameToEnum(token);

                    token = strtok(NULL, " ");
                    printf("%s\n", token);
                    currentSymbol = Symbol_New(strTrim(token, strlen(token) - 1), currentLinkDirection, currentLinkSymbolType);
                    

                    switch (currentLinkSymbolType)
                    {
                    case s_function_declaration:
                        ErrorAndExit(ERROR_INTERNAL, "Function declaration not yet supported!\n");
                        break;

                    case s_function_definition:
                        break;

                    case s_variable:
                        break;

                    case s_null:
                        ErrorAndExit(ERROR_INTERNAL, "Saw s_null as link symbol type!\n");
                        break;
                    }


                    if(currentLinkDirection == export)
                    {
                        addExport(exports, requires, currentSymbol);
                    }
                    else
                    {
                        addRequire(exports, requires, currentSymbol);
                    }
                }

                // printf("%s\n", token);
            }
            else
            {
                if (currentSymbol == NULL)
                {
                    ErrorAndExit(ERROR_INVOCATION, "Malformed input file - couldn't find directive!\n");
                }
                char *thisLine = malloc(len + 1);
                memcpy(thisLine, inBuf, len + 1);
                LinkedList_Append(currentSymbol->lines, thisLine);
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

    printf("Exports:\n");
    for(int i = 0; i < s_null; i++)
    {
        if(exports[i]->size > 0)
        {
            printf("%s:\n", symbolEnumToName(i));
            for(struct LinkedListNode *runner = exports[i]->head; runner != NULL; runner = runner->next)
            {
                struct Symbol *exported = runner->data;
                printf("\t%s\n", exported->name);
            }
        }
    }

    printf("Requirements not met:\n");
    for(int i = 0; i < s_null; i++)
    {
        if(requires[i]->size > 0)
        {
            printf("%s:\n", symbolEnumToName(i));
            for(struct LinkedListNode *runner = requires[i]->head; runner != NULL; runner = runner->next)
            {
                struct Symbol *missing = runner->data;
                printf("\t%s\n", missing->name);
            }
        }
    }
}