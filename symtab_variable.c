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
    type_deinit(&variable->type);
    free(variable);
}

void variable_entry_print(struct VariableEntry *variable, FILE *outFile, size_t depth)
{
    for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
    {
        fprintf(outFile, "\t");
    }
    char *typeName = type_get_name(&variable->type);
    fprintf(outFile, "%s %s\n", typeName, variable->name);
    free(typeName);
}

void variable_entry_try_resolve_generic(struct VariableEntry *variable, HashTable *paramsMap, char *resolvedStructName, List *resolvedParams)
{
    type_try_resolve_generic(&variable->type, paramsMap, resolvedStructName, resolvedParams);
    if(strcmp(variable->name, OUT_OBJECT_POINTER_NAME) == 0)
    {
        variable->type.pointerLevel = 1;
    }
}
