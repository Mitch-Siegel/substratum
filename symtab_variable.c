#include "symtab_variable.h"

#include "log.h"
#include "symtab_function.h"
#include "util.h"

// create a variable within the given scope
struct VariableEntry *create_variable(struct Scope *scope,
                                     struct AST *name,
                                     struct Type *type,
                                     u8 isGlobal,
                                     size_t declaredAt,
                                     u8 isArgument,
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
    newVariable->name = name->value;

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

    if (scope_contains(scope, name->value))
    {
        log_tree(LOG_FATAL, name, "Redifinition of symbol %s!", name->value);
    }

    // if we have an argument, it will be trivially spilled because it is passed in on the stack
    if (isArgument)
    {
        scope_insert(scope, name->value, newVariable, E_ARGUMENT, accessibility);
    }
    else
    {
        scope_insert(scope, name->value, newVariable, E_VARIABLE, accessibility);
    }

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

struct VariableEntry *lookup_var_by_string(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name);
    if (lookedUp == NULL)
    {
        InternalError("Lookup of variable [%s] by string name failed!", name);
    }

    switch (lookedUp->type)
    {
    case E_ARGUMENT:
    case E_VARIABLE:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up variable [%s]!", name);
    }
}

struct VariableEntry *lookup_var(struct Scope *scope, struct AST *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, name, "Use of undeclared variable '%s'", name->value);
    }

    switch (lookedUp->type)
    {
    case E_ARGUMENT:
    case E_VARIABLE:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up variable [%s]!", name->value);
    }
}
