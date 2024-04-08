#include "symtab_variable.h"

#include "symtab_function.h"
#include "util.h"

// create a variable within the given scope
struct VariableEntry *createVariable(struct Scope *scope,
                                     struct AST *name,
                                     struct Type *type,
                                     u8 isGlobal,
                                     size_t declaredAt,
                                     u8 isArgument)
{
    struct VariableEntry *newVariable = malloc(sizeof(struct VariableEntry));
    newVariable->type = *type;
    newVariable->stackOffset = 0;
    newVariable->mustSpill = 0;
    newVariable->name = name->value;

    newVariable->type.initializeTo = NULL;

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
        ErrorWithAST(ERROR_CODE, name, "Redifinition of symbol %s!\n", name->value);
    }

    // if we have an argument, it will be trivially spilled because it is passed in on the stack
    if (isArgument)
    {
        // compute the padding necessary for alignment of this argument
        scope->parentFunction->argStackSize += Scope_ComputePaddingForAlignment(scope, type, scope->parentFunction->argStackSize);

        // put our argument's offset at the newly-aligned stack size, then add the size of the argument to the argument stack size
        newVariable->stackOffset = scope->parentFunction->argStackSize;
        scope->parentFunction->argStackSize += getSizeOfType(scope, type);

        Scope_insert(scope, name->value, newVariable, e_argument);
    }
    else
    {
        Scope_insert(scope, name->value, newVariable, e_variable);
    }

    return newVariable;
}

struct VariableEntry *lookupVarByString(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name);
    if (lookedUp == NULL)
    {
        ErrorAndExit(ERROR_INTERNAL, "Lookup of variable [%s] by string name failed!\n", name);
    }

    switch (lookedUp->type)
    {
    case e_argument:
    case e_variable:
        return lookedUp->entry;

    default:
        ErrorAndExit(ERROR_INTERNAL, "Lookup returned unexpected symbol table entry type when looking up variable [%s]!\n", name);
    }
}

struct VariableEntry *lookupVar(struct Scope *scope, struct AST *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        ErrorWithAST(ERROR_CODE, name, "Use of undeclared variable '%s'\n", name->value);
    }

    switch (lookedUp->type)
    {
    case e_argument:
    case e_variable:
        return lookedUp->entry;

    default:
        ErrorAndExit(ERROR_INTERNAL, "Lookup returned unexpected symbol table entry type when looking up variable [%s]!\n", name->value);
    }
}
