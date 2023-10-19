#include "symbols.h"
#include "util.h"

#include <string.h>

int GetSizeOfType(struct Type *t)
{
    int size = 0;

    switch (t->basicType)
    {
    case vt_null:
        ErrorAndExit(ERROR_INTERNAL, "GetSizeOfType called with basic type of vt_null!\n");
        break;

    case vt_uint8:
        size = 1;
        break;

    case vt_uint16:
        size = 2;
        break;

    case vt_uint32:
        size = 4;
        break;

    case vt_class:
        ErrorAndExit(ERROR_INTERNAL, "GetSizeOfType called with basic type of vt_class!!\n");
    }

    if (t->arraySize > 0)
    {
        if (t->indirectionLevel > 1)
        {
            size = 4;
        }

        size *= t->arraySize;
    }
    else
    {
        if (t->indirectionLevel > 0)
        {
            size = 4;
        }
    }

    return size;
}

struct Symbol *Symbol_New(char *name, enum LinkDirection direction, enum LinkedSymbol symbolType, char *fromFile)
{
    struct Symbol *wip = malloc(sizeof(struct Symbol));
    wip->name = name;
    wip->direction = direction;
    wip->symbolType = symbolType;
    wip->lines = LinkedList_New();
    wip->linkerLines = LinkedList_New();
    wip->fromFile = fromFile;

    return wip;
}

void Symbol_Free(struct Symbol *s)
{
    LinkedList_Free(s->lines, free);
    LinkedList_Free(s->linkerLines, free);
    switch (s->symbolType)
    {
    case s_function_declaration:
    case s_function_definition:
    {
        if (s->data.asFunction.nArgs > 0)
        {
            free(s->data.asFunction.args);
        }
        free(s->data.asFunction.returnType);
        // don't free name as it lives in the dictionary
    }
    break;

    case s_section:
    case s_variable:
        break;

    case s_object:
        if (s->data.asObject.isInitialized)
        {
            free(s->data.asObject.initializeTo);
        }
        break;

    case s_null:
        ErrorAndExit(ERROR_INTERNAL, "Symbol_Free called for symbol with type null!\n");
        break;
    }
    free(s);
}

char *startup[] = {"li t0, 0x10000000",
                  "andi t1, t1, 0",
                  "addi t1, t1, 'h'",
                  "sw t1, 0(t0)",
                  "andi t1, t1, 0",
                  "addi t1, t1, 'e'",
                  "sw t1, 0(t0)",
                  "andi t1, t1, 0",
                  "addi t1, t1, 'l'",
                  "sw t1, 0(t0)",
                  "sw t1, 0(t0)",
                  "andi t1, t1, 0",
                  "addi t1, t1, 'o'",
                  "sw t1, 0(t0)",
                  "andi t1, t1, 0",
                  "addi t1, t1, '!'",
                  "sw t1, 0(t0)",
                  "andi t1, t1, 0",
                  "addi t1, t1, 10",
                  "sw t1, 0(t0)",
                  ""};

void Symbol_Write(struct Symbol *s, FILE *f, char outputExecutable)
{
    if (!outputExecutable)
    {
        for (struct LinkedListNode *rawRunner = s->linkerLines->head; rawRunner != NULL; rawRunner = rawRunner->next)
        {
            fputs(rawRunner->data, f);
            fputc('\n', f);
        }
    }

    else
    {
        switch (s->symbolType)

        {
        case s_variable:
            fprintf(f, "%s:\n", s->name);
            // only reserve space if this variable is not initialized
            // if it is initialized, the data directives we will output later will reserve the space on their own
            if (s->data.asVariable.initializeTo == NULL)
            {
                fprintf(f, "#res %d\n", GetSizeOfType(&s->data.asVariable));
            }
            break;

        case s_function_declaration:
            break;

        case s_function_definition:
        {
        }
        break;

        case s_section:
        {
        }
        break;

        case s_object:
            fprintf(f, "%s:\n", s->name);
            if (s->data.asObject.isInitialized)
            {
                fputs("#d8 ", f);
                for (int i = 0; i < s->data.asObject.size; i++)
                {
                    fprintf(f, "0x%02x", s->data.asObject.initializeTo[i]);
                    if (i < s->data.asObject.size - 1)
                    {
                        fputs(", ", f);
                    }
                }
                fputs("\n", f);
            }
            else
            {
                fprintf(f, "\t#res %d\n", s->data.asObject.size);
            }
            break;

        case s_null:
            ErrorAndExit(ERROR_INTERNAL, "Got null symbol type in Symbol_Write!\n");
            break;
        }
    }

    for (struct LinkedListNode *rawRunner = s->lines->head; rawRunner != NULL; rawRunner = rawRunner->next)
    {
        fputs(rawRunner->data, f);
        fputc('\n', f);
    }

    if (outputExecutable &&
        (s->symbolType == s_section) &&
        (!strcmp(s->name, "userstart")))
    {
        fprintf(f, "userstart:\n");
        for(int i = 0; startup[i][0] != '\0'; i++)
        {
            fprintf(f, "\t%s\n", startup[i]);
        }
        // fprintf(f, "\tcall main\n");
        fprintf(f, "pgm_done:\n\twfi\n\tbeq t1, t1, pgm_done\n");
    }
}

enum LinkedSymbol symbolNameToEnum(char *name)
{
    if (!strcmp(name, "funcdef"))
    {
        return s_function_definition;
    }
    else if (!strcmp(name, "funcdec"))
    {
        return s_function_declaration;
    }
    else if (!strcmp(name, "variable"))
    {
        return s_variable;
    }
    else if (!strcmp(name, "section"))
    {
        return s_section;
    }
    else if (!strcmp(name, "object"))
    {
        return s_object;
    }
    else
    {
        ErrorAndExit(ERROR_INTERNAL, "Unexpected symbol name %s\n", name);
    }
}

char *symbolEnumToName(enum LinkedSymbol s)
{
    switch (s)
    {
    case s_function_declaration:
        return "funcdec";

    case s_function_definition:
        return "funcdef";

    case s_variable:
        return "variable";

    case s_section:
        return "section";

    case s_object:
        return "object";

    case s_null:
        return "s_null";
    }

    return "s_null";
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

char addRequire(struct LinkedList **exports, struct LinkedList **requires, struct Symbol *toRequire)
{
    enum LinkedSymbol symbolType = toRequire->symbolType;
    if (LinkedList_Find(requires[symbolType], compareSymbols, toRequire) == NULL)
    {
        LinkedList_Append(requires[symbolType], toRequire);
        return 0;
    }
    else
    {
        return 1;
    }
}

// returns 0 if added, 1 if not
char addExport(struct LinkedList **exports, struct LinkedList **requires, struct Symbol *toAdd)
{
    enum LinkedSymbol addType = toAdd->symbolType;

    // check for duplicates of anything except function declarations
    struct Symbol *found;
    if ((addType != s_function_declaration) &&
        (found = LinkedList_Find(exports[addType], compareSymbols, toAdd)))
    {
        // if we see something other than a section duplicated, throw an error
        if ((addType != s_section))
        {
            ErrorAndExit(ERROR_CODE, "Multiple definition of symbol %s %s - from %s and %s!\n", symbolEnumToName(addType), toAdd->name, found->fromFile, toAdd->fromFile);
        }

        LinkedList_Join(found->lines, toAdd->lines);

        // printf("\n\n\n\ndelete duplicate start lines\n");
        // while (LinkedList_Find(found->lines, strcmp, "START:"))
        // {
        // LinkedList_Delete(found->lines, strcmp, "START:");
        // }

        return 1;
    }

    LinkedList_Append(exports[addType], toAdd);

    // if we export a declaration, we must require a definition for it as well
    if (addType == s_function_declaration)
    {
        struct Symbol *funcDefRequired = Symbol_New(toAdd->name, require, s_function_definition, toAdd->fromFile);
        funcDefRequired->data.asFunction = toAdd->data.asFunction;

        if (toAdd->data.asFunction.nArgs)
        {
            funcDefRequired->data.asFunction.args = malloc(toAdd->data.asFunction.nArgs * sizeof(struct Type));
        }
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

        if (addRequire(exports, requires, funcDefRequired))
        {
            Symbol_Free(funcDefRequired);
        }
    }

    // if adding this export satisfies any requires, delete them
    if (LinkedList_Find(requires[addType], compareSymbols, toAdd) != NULL)
    {
        struct Symbol *deletedRequire = LinkedList_Delete(requires[addType], compareSymbols, toAdd);
        Symbol_Free(deletedRequire);
    }

    return 0;
}