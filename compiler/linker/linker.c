
#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "symbols.h"

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

size_t getline_force_raw(char **linep, size_t *linecapp, FILE *stream)
{
    size_t len = getline(linep, linecapp, stream);
    if (len == 0)
    {
        ErrorAndExit(ERROR_INTERNAL, "getline_force expects non-zero line length, got 0!\n");
    }
    if ((*linep)[len - 1] == '\n')
    {
        (*linep)[len - 1] = '\0';
        len--;
    }
    return len;
}

size_t getline_force(char **linep, size_t *linecapp, FILE *stream, struct Symbol *currentSymbol)
{
    size_t len = getline_force_raw(linep, linecapp, stream);
    LinkedList_Append(currentSymbol->lines, strTrim(*linep, len));
    return len;
}

size_t getline_force_metadata(char **linep, size_t *linecapp, FILE *stream, struct Symbol *currentSymbol)
{
    size_t len = getline_force_raw(linep, linecapp, stream);
    LinkedList_Append(currentSymbol->linkerLines, strTrim(*linep, len));
    return len;
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
            len = getline_force_raw(&inBuf, &bufSize, inFile);
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
                    {
                        len = getline_force_metadata(&inBuf, &bufSize, inFile, currentSymbol);
                        char *token = strtok(inBuf, " ");
                        if (strcmp(token, "returns"))
                        {
                            ErrorAndExit(ERROR_INTERNAL, "Expected returns [type] but got %s instead!\n", token);
                        }

                        currentSymbol->data.asFunction.returnType = parseType(inBuf + strlen(token) + 1);

                        len = getline_force_metadata(&inBuf, &bufSize, inFile, currentSymbol);
                        token = strtok(inBuf, " ");

                        int nArgs = atoi(token);
                        currentSymbol->data.asFunction.nArgs = nArgs;

                        currentSymbol->data.asFunction.args = malloc(nArgs * sizeof(struct Type));

                        token = strtok(NULL, " ");
                        if (strcmp(token, "arguments"))
                        {
                            ErrorAndExit(ERROR_INTERNAL, "Expected '[n] arguments' but got %s %s instead!\n", inBuf, token);
                        }

                        while (nArgs-- > 0)
                        {
                            len = getline_force_metadata(&inBuf, &bufSize, inFile, currentSymbol);
                        }
                    }
                    break;

                    case s_variable:
                        break;

                    case s_section:
                        break;

                    case s_null:
                        ErrorAndExit(ERROR_INTERNAL, "Saw s_null as link symbol type!\n");
                        break;
                    }

                    if (currentLinkDirection == export)
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
            else // copying data, just stick the line onto the existing WIP symbol
            {
                if (currentSymbol == NULL)
                {
                    ErrorAndExit(ERROR_INVOCATION, "Malformed input file - couldn't find directive!\n");
                }
                LinkedList_Append(currentSymbol->lines, strTrim(inBuf, len));
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

    printf("Requirements not met:\n");
    for (int i = 0; i < s_null; i++)
    {
        if (requires[i] -> size > 0)
        {
            printf("%s(s):\n", symbolEnumToName(i));
            for (struct LinkedListNode *runner = requires[i] -> head; runner != NULL; runner = runner->next)
            {
                struct Symbol *missing = runner->data;
                printf("\t%s\n", missing->name);
            }
        }
    }

    printf("Exports:\n");
    for (int i = 0; i < s_null; i++)
    {
        if (exports[i]->size > 0)
        {
            printf("%s(s):\n", symbolEnumToName(i));
            for (struct LinkedListNode *runner = exports[i]->head; runner != NULL; runner = runner->next)
            {
                struct Symbol *exported = runner->data;
                printf("\t%s\n", exported->name);
                for (struct LinkedListNode *rawRunner = exported->linkerLines->head; rawRunner != NULL; rawRunner = rawRunner->next)
                {
                    printf("%s ", (char *)rawRunner->data);
                }
                printf("\n");
                for (struct LinkedListNode *rawRunner = exported->lines->head; rawRunner != NULL; rawRunner = rawRunner->next)
                {
                    printf("\t\t%s\n", (char *)rawRunner->data);
                }
            }
        }
    }
}