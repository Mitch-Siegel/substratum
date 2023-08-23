
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

void addRequire(struct LinkedList **exports, struct LinkedList **requires, struct Symbol *toRequire)
{
    enum LinkedSymbol symbolType = toRequire->symbolType;
    if (LinkedList_Find(requires[symbolType], compareSymbols, toRequire) == NULL)
    {
        LinkedList_Append(requires[symbolType], toRequire);
    }
    else
    {
        Symbol_Free(toRequire);
    }
}

void addExport(struct LinkedList **exports, struct LinkedList **requires, struct Symbol *toAdd)
{
    enum LinkedSymbol addType = toAdd->symbolType;

    // check for duplicates of anything except function declarations
    struct Symbol *found;
    if ((addType != s_function_declaration) &&
        (found = LinkedList_Find(exports[addType], compareSymbols, toAdd)))
    {
        // if we see something other than section userstart duplicated, throw an error
        if ((strcmp(toAdd->name, "userstart") || (addType != s_section)))
        {

            ErrorAndExit(ERROR_CODE, "Multiple definition of symbol %s %s - from %s and %s!\n", symbolEnumToName(addType), toAdd->name, found->fromFile, toAdd->fromFile);
        }

        LinkedList_Join(found->lines, toAdd->lines);

        // printf("\n\n\n\ndelete duplicate start lines\n");
        // while (LinkedList_Find(found->lines, strcmp, "START:"))
        // {
        // LinkedList_Delete(found->lines, strcmp, "START:");
        // }

        // Symbol_Free(toAdd);
        return;
    }

    LinkedList_Append(exports[addType], toAdd);

    // if we export a declaration, we must require a definition for it as well
    if (addType == s_function_declaration)
    {
        struct Symbol *funcDefRequired = Symbol_New(toAdd->name, require, s_function_definition, toAdd->fromFile);
        funcDefRequired->data.asFunction = toAdd->data.asFunction;

        funcDefRequired->data.asFunction.args = malloc(toAdd->data.asFunction.nArgs * sizeof(struct Type));
        memcpy(funcDefRequired->data.asFunction.args, toAdd->data.asFunction.args, toAdd->data.asFunction.nArgs * sizeof(struct Type));

        funcDefRequired->data.asFunction.returnType = malloc(sizeof(struct Type));
        memcpy(funcDefRequired->data.asFunction.returnType, toAdd->data.asFunction.returnType, sizeof(struct Type));

        for (struct LinkedListNode *runner = toAdd->lines->head; runner != NULL; runner = runner->next)
        {
            LinkedList_Append(funcDefRequired->lines, strdup(runner->data));
        }

        for (struct LinkedListNode *runner = toAdd->linkerLines->head; runner != NULL; runner = runner->next)
        {
            LinkedList_Append(funcDefRequired->linkerLines, strdup(runner->data));
        }

        addRequire(exports, requires, funcDefRequired);
    }

    // if adding this export satisfies any requires, delete them    
    if (LinkedList_Find(requires[addType], compareSymbols, toAdd) != NULL)
    {
        printf("delete require for symbol %s %s\n", symbolEnumToName(addType), toAdd->name);
        struct Symbol *deletedRequire = LinkedList_Delete(requires[addType], compareSymbols, toAdd);
        Symbol_Free(deletedRequire);
    }

}

int getline_force_raw(char **linep, size_t *linecapp, FILE *stream)
{
    int len = getline(linep, linecapp, stream);
    if (len == 0)
    {
        ErrorAndExit(ERROR_INTERNAL, "getline_force expects non-zero line length, got 0!\n");
    }
    if (len == -1)
    {
        return len;
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
    struct Dictionary *symbolNames = Dictionary_New(10);
    char *outFileName = NULL;
    FILE *outFile;

    int c;
    char outputExecutable = 0;

    opterr = 0;

    while ((c = getopt(argc, argv, "i:o:e")) != -1)
        switch (c)
        {
        case 'i':
            Dictionary_LookupOrInsert(inputFiles, optarg);
            break;

        case 'o':
            outFileName = strTrim(optarg, strlen(optarg));
            break;

        case 'e':
            outputExecutable = 1;
            break;

        default:
            ErrorAndExit(ERROR_INVOCATION, "Arguments:\n-i `filename`:\tinput file\n-o: `filename`:\toutput file\n-e:\toutput as executable instead of object file");
        }

    if (inputFiles->buckets[0]->size == 0)
    {
        ErrorAndExit(ERROR_INVOCATION, "No input files provided!\n");
    }

    if (outFileName == NULL)
    {
        ErrorAndExit(ERROR_INVOCATION, "No output file provided!\n");
    }

    printf("have %d input files\n", inputFiles->buckets[0]->size);

    char *inBuf = NULL;
    size_t bufSize = 0;

    for (struct LinkedListNode *inFileName = inputFiles->buckets[0]->head; inFileName != NULL; inFileName = inFileName->next)
    {
        printf("opening input file %s\n", (char *)inFileName->data);
        FILE *inFile = fopen(inFileName->data, "rb");
        if (inFile == NULL)
        {
            ErrorAndExit(ERROR_INTERNAL, "Error opening file %s\n", (char *)inFileName->data);
        }

        int nSymbols[s_null];
        memset(nSymbols, 0, s_null * sizeof(int));

        if (outputExecutable)
        {
            struct Symbol *main = Symbol_New(Dictionary_LookupOrInsert(symbolNames, "main"), require, s_function_definition, inFileName->data);
            main->data.asFunction.nArgs = 0;

            struct Type *mainReturnType = malloc(sizeof(struct Type));
            mainReturnType->isPrimitive = 1;
            mainReturnType->size = 0;
            mainReturnType->indirectionLevel = 0;

            main->data.asFunction.returnType = mainReturnType;

            addRequire(exports, requires, main);
        }

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
                    if (symbolNameToEnum(token) != currentLinkSymbolType)
                    {
                        ErrorAndExit(ERROR_INTERNAL, "End symbol type doesn't match start!\n");
                    }

                    token = strtok(NULL, " ");
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

                    token = strtok(NULL, " ");
                    currentLinkSymbolType = symbolNameToEnum(token);

                    token = strtok(NULL, " ");

                    currentSymbol = Symbol_New(Dictionary_LookupOrInsert(symbolNames, token), currentLinkDirection, currentLinkSymbolType, inFileName->data);

                    switch (currentLinkSymbolType)
                    {
                    case s_function_declaration:
                        // ErrorAndExit(ERROR_INTERNAL, "Function declaration not yet supported!\n");
                        // break;
                        // fall through to function definition as the symbol data is identical for declaration and definition

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

                        if (nArgs > 0)
                        {
                            currentSymbol->data.asFunction.args = malloc(nArgs * sizeof(struct Type));
                        }

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
                        {
                            getline_force_metadata(&inBuf, &bufSize, inFile, currentSymbol);
                            struct Type *varType = parseType(inBuf);
                            currentSymbol->data.asVariable = *varType;
                            free(varType);
                        }
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
                    nSymbols[currentLinkSymbolType]++;
                }
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

        printf("\t");
        for (int i = 0; i < s_null; i++)
        {
            printf("%d %s ", nSymbols[i], symbolEnumToName(i));
        }
        printf("\n");
    }

    free(inBuf);
    printf("\n");

    outFile = fopen(outFileName, "wb");
    if (outFile == NULL)
    {
        ErrorAndExit(ERROR_INTERNAL, "Error opening file %s for output\n", outFileName);
    }

    free(outFileName);

    int nOutputRequirements = 0;
    for (int i = 0; i < s_null; i++)
    {
        if (requires[i] -> size > 0)
        {
            for (struct LinkedListNode *runner = requires[i] -> head; runner != NULL; runner = runner->next)
            {
                nOutputRequirements++;
            }
        }
    }

    if (nOutputRequirements)
    {
        if (outputExecutable)
        {
            printf("Missing definitions:\n");
            for (int i = 0; i < s_null; i++)
            {
                if (requires[i] -> size > 0)
                {
                    for (struct LinkedListNode *runner = requires[i] -> head; runner != NULL; runner = runner->next)
                    {
                        struct Symbol *required = runner->data;
                        printf("\t%s %s\n", symbolEnumToName(required->symbolType), required->name);
                        // Symbol_Write(required, stdout, 0);
                    }
                }
            }
            printf("\n");
            ErrorAndExit(ERROR_INVOCATION, "Unable to create executable - %d requirements not satisfied!\n", nOutputRequirements);
        }

        for (int i = 0; i < s_null; i++)
        {
            if (requires[i] -> size > 0)
            {
                for (struct LinkedListNode *runner = requires[i] -> head; runner != NULL; runner = runner->next)
                {
                    struct Symbol *required = runner->data;
                    fprintf(outFile, "~require %s %s\n", symbolEnumToName(required->symbolType), required->name);
                    Symbol_Write(required, outFile, 0);
                    fprintf(outFile, "~end require %s %s\n", symbolEnumToName(required->symbolType), required->name);
                }
            }
        }
    }

    if (outputExecutable)
    {
        fprintf(outFile, "#include \"CPU.asm\"\nentry START\n");
    }

    for (int i = 0; i < s_null; i++)
    {
        // printf("%d %s(s)\n", exports[i]->size, symbolEnumToName(i));
        if (exports[i]->size > 0)
        {
            for (struct LinkedListNode *runner = exports[i]->head; runner != NULL; runner = runner->next)
            {
                struct Symbol *exported = runner->data;
                if (!outputExecutable)
                {
                    fprintf(outFile, "~export %s %s\n", symbolEnumToName(exported->symbolType), exported->name);
                }

                Symbol_Write(exported, outFile, outputExecutable);
                if (!outputExecutable)
                {
                    fprintf(outFile, "~end export %s %s\n", symbolEnumToName(exported->symbolType), exported->name);
                }
            }
        }
    }

    for (int i = 0; i < s_null; i++)
    {
        LinkedList_Free(exports[i], Symbol_Free);
        LinkedList_Free(requires[i], Symbol_Free);
    }

    Dictionary_Free(symbolNames);
    Dictionary_Free(inputFiles);
}
