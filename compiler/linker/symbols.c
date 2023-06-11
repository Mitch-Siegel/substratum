#include "symbols.h"
#include "util.h"

#include <string.h>

struct Symbol *Symbol_New(char *name, enum LinkDirection direction, enum LinkedSymbol symbolType)
{
    struct Symbol *wip = malloc(sizeof(struct Symbol));
    wip->name = name;
    wip->direction = direction;
    wip->symbolType = symbolType;
    wip->lines = LinkedList_New();
    wip->linkerLines = LinkedList_New();

    return wip;
}

void Symbol_Free(struct Symbol *s)
{
    LinkedList_Free(s->lines, free);
    free(s);
}

enum LinkedSymbol symbolNameToEnum(char *name)
{
    if (!strcmp(name, "funcdef"))
    {
        return s_function_definition;
    }
    else if (!strcmp(name, "variable"))
    {
        return s_variable;
    }
    else if (!strcmp(name, "section"))
    {
        return s_section;
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

    case s_null:
        return "s_null";
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

struct Type *parseType(char *declString)
{
    char *lasts = NULL;

    struct Type *parsed = malloc(sizeof(struct Type));
    char *token = strtok_r(declString, " ", &lasts);
    int typeNum = atoi(token);

    if (typeNum < 4)
    {
        parsed->isPrimitive = 1;
        parsed->data.primitive = typeNum;
    }
    else
    {
        parsed->isPrimitive = 0;
        ErrorAndExit(ERROR_INTERNAL, "non-primitive types not yet supported!\n");
    }

    token = strtok_r(NULL, " ", &lasts);
    if (token[strlen(token) - 1] != '*')
    {
        ErrorAndExit(ERROR_INTERNAL, "Expected to see * indicating dereference level of type at end of type string, got %s instead!\n", token);
    }
    token[strlen(token - 1)] = '\0';
    parsed->indirectionLevel = atoi(token);

    return parsed;
}