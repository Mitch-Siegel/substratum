#include "symtab_variable.h"

#include "log.h"
#include "symtab_function.h"
#include "util.h"

// TODO: examine isGlobal - can it be related to scope->parentscope instead?
struct VariableEntry *variable_entry_new(char *name,
                                         struct Type *type,
                                         bool isGlobal,
                                         bool isArgument,
                                         enum ACCESS accessibility)
{
    if (isArgument && (accessibility != A_PUBLIC))
    {
        InternalError("createVariable called with isArgument == 1 and accessibility != a_public - illegal arguments");
    }

    struct VariableEntry *newVariable = malloc(sizeof(struct VariableEntry));
    newVariable->type = *type;
    newVariable->stackOffset = 0;
    newVariable->mustSpill = 0;
    newVariable->name = name;

    if (isGlobal)
    {
        newVariable->isGlobal = 1;
    }
    else
    {
        newVariable->isGlobal = 0;
    }

    // don't take these as arguments as they will only ever be set for specific declarations
    newVariable->isExtern = 0;
    newVariable->isStringLiteral = 0;

    return newVariable;
}

void variable_entry_free(struct VariableEntry *variable)
{
    struct Type *variableType = &variable->type;
    if (variableType->basicType == VT_ARRAY)
    {
        struct Type typeRunner = *variableType;
        while (typeRunner.basicType == VT_ARRAY)
        {
            if (typeRunner.array.initializeArrayTo != NULL)
            {
                for (size_t i = 0; i < typeRunner.array.size; i++)
                {
                    free(typeRunner.array.initializeArrayTo[i]);
                }
                free(typeRunner.array.initializeArrayTo);
            }
            typeRunner = *typeRunner.array.type;
        }
    }
    else
    {
        if (variableType->nonArray.initializeTo != NULL)
        {
            free(variableType->nonArray.initializeTo);
        }
    }
    free(variable);
}
