#include "symtab_scope.h"

#include "log.h"
#include "symtab_basicblock.h"
#include "symtab_enum.h"
#include "symtab_function.h"
#include "symtab_struct.h"
#include "symtab_variable.h"
#include "util.h"

extern struct Dictionary *parseDict;
/*
 * Scope functions
 *
 */
struct Scope *scope_new(struct Scope *parentScope, char *name, struct FunctionEntry *parentFunction, struct StructEntry *parentImpl)
{
    struct Scope *wip = malloc(sizeof(struct Scope));
    wip->entries = stack_new();

    wip->parentScope = parentScope;
    wip->parentFunction = parentFunction;
    wip->parentImpl = parentImpl;
    wip->name = name;
    wip->subScopeCount = 0;
    return wip;
}

void scope_free(struct Scope *scope)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *examinedEntry = scope->entries->data[entryIndex];
        switch (examinedEntry->type)
        {
        case E_SCOPE:
            scope_free(examinedEntry->entry);
            break;

        case E_FUNCTION:
            function_entry_free(examinedEntry->entry);
            break;

        case E_VARIABLE:
        case E_ARGUMENT:
            variable_entry_free(examinedEntry->entry);
            break;

        case E_STRUCT:
            struct_entry_free(examinedEntry->entry);
            break;

        case E_ENUM:
            enum_entry_free(examinedEntry->entry);
            break;

        case E_BASICBLOCK:
            basic_block_free(examinedEntry->entry);
            break;
        }

        free(examinedEntry);
    }
    stack_free(scope->entries);
    free(scope);
}

// insert a member with a given name and pointer to entry, along with info about the entry type
void scope_insert(struct Scope *scope, char *name, void *newEntry, enum SCOPE_MEMBER_TYPE type, enum ACCESS accessibility)
{
    if (scope_contains(scope, name))
    {
        InternalError("Error defining symbol [%s] - name already exists!", name);
    }
    struct ScopeMember *wip = malloc(sizeof(struct ScopeMember));
    wip->name = name;
    wip->entry = newEntry;
    wip->type = type;
    wip->accessibility = accessibility;
    stack_push(scope->entries, wip);
}

// create and return a child scope of the scope provided as an argument
struct Scope *scope_create_sub_scope(struct Scope *parent_scope)
{
    if (parent_scope->subScopeCount == U8_MAX)
    {
        InternalError("Too many subscopes of scope %s", parent_scope->name);
    }
    char *helpStr = malloc(2 + strlen(parent_scope->name) + 1);
    sprintf(helpStr, "%02x", parent_scope->subScopeCount);
    char *newScopeName = dictionary_lookup_or_insert(parseDict, helpStr);
    free(helpStr);
    parent_scope->subScopeCount++;

    struct Scope *newScope = scope_new(parent_scope, newScopeName, parent_scope->parentFunction, parent_scope->parentImpl);

    scope_insert(parent_scope, newScopeName, newScope, E_SCOPE, A_PUBLIC);
    return newScope;
}

// Scope lookup functions

char scope_contains(struct Scope *scope, char *name)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        if (!strcmp(name, ((struct ScopeMember *)scope->entries->data[entryIndex])->name))
        {
            return 1;
        }
    }
    return 0;
}

// if a member with the given name exists in this scope or any of its parents, return it
// also looks up entries from deeper scopes, but only as their mangled names specify
struct ScopeMember *scope_lookup(struct Scope *scope, char *name)
{
    while (scope != NULL)
    {
        for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
        {
            struct ScopeMember *examinedEntry = scope->entries->data[entryIndex];
            if (!strcmp(examinedEntry->name, name))
            {
                return examinedEntry;
            }
        }
        scope = scope->parentScope;
    }
    return NULL;
}
