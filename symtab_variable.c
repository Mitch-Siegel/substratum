#include "symtab_variable.h"

#include "log.h"
#include "symtab_function.h"
#include "util.h"

// create a variable within the given scope
struct VariableEntry *createVariable(struct Scope *scope,
                                     struct AST *name,
                                     struct Type *type,
                                     u8 isGlobal,
                                     size_t declaredAt,
                                     u8 isArgument,
                                     enum Access accessibility)
{
    if (isArgument && (accessibility != a_public))
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

    if (Scope_contains(scope, name->value))
    {
        LogTree(LOG_FATAL, name, "Redifinition of symbol %s!", name->value);
    }

    // if we have an argument, it will be trivially spilled because it is passed in on the stack
    if (isArgument)
    {
        Scope_insert(scope, name->value, newVariable, e_argument, accessibility);
    }
    else
    {
        Scope_insert(scope, name->value, newVariable, e_variable, accessibility);
    }

    return newVariable;
}

void VariableEntry_free(struct VariableEntry *variable)
{
    struct Type *variableType = &variable->type;
    if (variableType->basicType == vt_array)
    {
        struct Type typeRunner = *variableType;
        while (typeRunner.basicType == vt_array)
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

struct VariableEntry *lookupVarByString(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name);
    if (lookedUp == NULL)
    {
        InternalError("Lookup of variable [%s] by string name failed!", name);
    }

    switch (lookedUp->type)
    {
    case e_argument:
    case e_variable:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up variable [%s]!", name);
    }
}

struct VariableEntry *lookupVar(struct Scope *scope, struct AST *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        LogTree(LOG_FATAL, name, "Use of undeclared variable '%s'", name->value);
    }

    switch (lookedUp->type)
    {
    case e_argument:
    case e_variable:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up variable [%s]!", name->value);
    }
}
