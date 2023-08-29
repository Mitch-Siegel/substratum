
#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "symbols.h"
#include "input.h"

void linkerParseFile(FILE *inFile, char *inFileName, struct Dictionary *symbolNames, struct LinkedList *exports[s_null], struct LinkedList *requires[s_null])
{
    int nSymbols[s_null];
    memset(nSymbols, 0, s_null * sizeof(int));

    int len;
    char requireNewSymbol = 1;

    char currentLinkDirection = -1;
    enum LinkedSymbol currentLinkSymbolType = s_null;
    struct Symbol *currentSymbol = NULL;

    char ignoreDuplicateSymbol = 0;
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

                if(ignoreDuplicateSymbol)
                {
                    Symbol_Free(currentSymbol);
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

                currentSymbol = Symbol_New(Dictionary_LookupOrInsert(symbolNames, token), currentLinkDirection, currentLinkSymbolType, inFileName);

                switch (currentLinkSymbolType)
                {
                case s_function_declaration:
                    // fall through to function definition as the symbol data is identical for declaration and definition
                case s_function_definition:
                {
                    printf("function dec/def\n");
                    parseFunctionDeclaration(currentSymbol, inFile);
                }
                break;

                case s_variable:
                {
                    printf("variable\n");
                    parseVariable(currentSymbol, inFile);
                }
                break;

                case s_section:
                    break;

                case s_object:
                {
                    printf("object\n");
                    parseObject(currentSymbol, inFile);
                }
                break;

                case s_null:
                    ErrorAndExit(ERROR_INTERNAL, "Saw s_null as link symbol type!\n");
                    break;
                }

                if (currentLinkDirection == export)
                {
                    ignoreDuplicateSymbol = addExport(exports, requires, currentSymbol);
                }
                else
                {
                    addRequire(exports, requires, currentSymbol);
                    ignoreDuplicateSymbol = 0;
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

int main(int argc, char **argv)
{
    struct LinkedList *exports[s_null];
    struct LinkedList *
        requires[
            s_null];

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

    if (outputExecutable)
    {
        struct Symbol *main = Symbol_New(Dictionary_LookupOrInsert(symbolNames, "main"), require, s_function_definition, "");
        main->data.asFunction.nArgs = 0;

        struct Type *mainReturnType = malloc(sizeof(struct Type));
        mainReturnType->isPrimitive = 1;
        mainReturnType->size = 0;
        mainReturnType->indirectionLevel = 0;

        main->data.asFunction.returnType = mainReturnType;

        addRequire(exports, requires, main);
    }

    for (struct LinkedListNode *inFileName = inputFiles->buckets[0]->head; inFileName != NULL; inFileName = inFileName->next)
    {
        printf("opening input file %s\n", (char *)inFileName->data);
        FILE *inFile = fopen(inFileName->data, "rb");
        if (inFile == NULL)
        {
            ErrorAndExit(ERROR_INTERNAL, "Error opening file %s\n", (char *)inFileName->data);
        }

        linkerParseFile(inFile, (char *)inFileName->data, symbolNames, exports, requires);
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
        if (requires[i]->size > 0)
        {
            for (struct LinkedListNode *runner = requires[i]->head; runner != NULL; runner = runner->next)
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
                if (requires[i]->size > 0)
                {
                    for (struct LinkedListNode *runner = requires[i]->head; runner != NULL; runner = runner->next)
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
            if (requires[i]->size > 0)
            {
                for (struct LinkedListNode *runner = requires[i]->head; runner != NULL; runner = runner->next)
                {
                    struct Symbol *required = runner->data;
                    fprintf(outFile, "~require %s %s\n", symbolEnumToName(required->symbolType), required->name);
                    Symbol_Write(required, outFile, outputExecutable);
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
        printf("%d %s(s)\n", exports[i]->size, symbolEnumToName(i));
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
                printf("\t%s\n", exported->name);
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
