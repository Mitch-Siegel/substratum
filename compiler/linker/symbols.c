#include "symbols.h"
#include "util.h"
#include "tac.h" // for variableTypes

#include <string.h>

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
        if(s->data.asObject.isInitialized)
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
            fprintf(f, "%s:\n#res %d\n", s->name, s->data.asVariable.size);
            break;

        case s_function_declaration:
            break;

        case s_function_definition:
        {
            fputs("#align 2048\n", f);
        }
        break;

        case s_section:
            break;

        case s_object:
            fprintf(f, "%s:\n", s->name);
            if(s->data.asObject.isInitialized)
            {
                fputs("#d8 ", f);
                for(int i = 0; i < s->data.asObject.size; i++)
                {
                    fprintf(f, "0x%02x", s->data.asObject.initializeTo[i]);
                    if(i < s->data.asObject.size - 1)
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

struct Type *parseType(char *declString)
{
    char *lasts = NULL;

    struct Type *parsed = malloc(sizeof(struct Type));
    char *token = strtok_r(declString, " ", &lasts);
    int typeNum = atoi(token);

    switch ((enum variableTypes)typeNum)
    {
    case vt_null:
        parsed->size = 0;
        break;

    case vt_uint8:
        parsed->size = 1;
        break;

    case vt_uint16:
        parsed->size = 2;
        break;

    case vt_uint32:
        parsed->size = 4;
        break;

    default:
    {
        parsed->isPrimitive = 0;
        ErrorAndExit(ERROR_INTERNAL, "non-primitive types not yet supported!\n");
    }
    break;
    }

    token = strtok_r(NULL, " ", &lasts);
    if (token[strlen(token) - 1] != '*')
    {
        ErrorAndExit(ERROR_INTERNAL, "Expected to see * indicating dereference level of type at end of type string, got %s instead!\n", token);
    }
    token[strlen(token - 1)] = '\0';
    parsed->indirectionLevel = atoi(token);
    if (parsed->indirectionLevel)
    {
        parsed->size = 4;
    }

    return parsed;
}