#include "symtab_scope.h"

#include "log.h"
#include "symtab_basicblock.h"
#include "symtab_class.h"
#include "symtab_function.h"
#include "symtab_variable.h"
#include "util.h"

extern struct Dictionary *parseDict;
/*
 * Scope functions
 *
 */
struct Scope *Scope_new(struct Scope *parentScope, char *name, struct FunctionEntry *parentFunction)
{
    struct Scope *wip = malloc(sizeof(struct Scope));
    wip->entries = Stack_New();

    wip->parentFunction = parentFunction;
    wip->parentScope = parentScope;
    wip->name = name;
    wip->subScopeCount = 0;
    return wip;
}

void Scope_free(struct Scope *scope)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *examinedEntry = scope->entries->data[entryIndex];
        switch (examinedEntry->type)
        {
        case e_scope:
            Scope_free(examinedEntry->entry);
            break;

        case e_function:
            FunctionEntry_free(examinedEntry->entry);
            break;

        case e_variable:
        case e_argument:
            VariableEntry_free(examinedEntry->entry);
            break;

        case e_class:
            ClassEntry_free(examinedEntry->entry);
            break;

        case e_basicblock:
            BasicBlock_free(examinedEntry->entry);
            break;
        }

        free(examinedEntry);
    }
    Stack_Free(scope->entries);
    free(scope);
}

// insert a member with a given name and pointer to entry, along with info about the entry type
void Scope_insert(struct Scope *scope, char *name, void *newEntry, enum ScopeMemberType type, enum Access accessibility)
{
    if (Scope_contains(scope, name))
    {
        InternalError("Error defining symbol [%s] - name already exists!", name);
    }
    struct ScopeMember *wip = malloc(sizeof(struct ScopeMember));
    wip->name = name;
    wip->entry = newEntry;
    wip->type = type;
    wip->accessibility = accessibility;
    Stack_Push(scope->entries, wip);
}

// create and return a child scope of the scope provided as an argument
struct Scope *Scope_createSubScope(struct Scope *parentScope)
{
    if (parentScope->subScopeCount == U8_MAX)
    {
        InternalError("Too many subscopes of scope %s", parentScope->name);
    }
    char *helpStr = malloc(2 + strlen(parentScope->name) + 1);
    sprintf(helpStr, "%02x", parentScope->subScopeCount);
    char *newScopeName = Dictionary_LookupOrInsert(parseDict, helpStr);
    free(helpStr);
    parentScope->subScopeCount++;

    struct Scope *newScope = Scope_new(parentScope, newScopeName, parentScope->parentFunction);
    newScope->parentFunction = parentScope->parentFunction;

    Scope_insert(parentScope, newScopeName, newScope, e_scope, a_public);
    return newScope;
}

// Scope lookup functions

char Scope_contains(struct Scope *scope, char *name)
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
struct ScopeMember *Scope_lookup(struct Scope *scope, char *name)
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
