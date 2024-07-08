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

// create a variable within the given scope
struct VariableEntry *scope_create_variable(struct Scope *scope,
                                            struct Ast *name,
                                            struct Type *type,
                                            bool isGlobal,
                                            enum ACCESS accessibility)
{

    struct VariableEntry *newVariable = variable_entry_new(name->value, type, isGlobal, false, accessibility);

    if (scope_contains(scope, name->value))
    {
        log_tree(LOG_FATAL, name, "Redifinition of symbol %s!", name->value);
    }

    scope_insert(scope, name->value, newVariable, E_VARIABLE, accessibility);

    return newVariable;
}

// create an argument within the given scope
struct VariableEntry *scope_create_argument(struct Scope *scope,
                                            struct Ast *name,
                                            struct Type *type,
                                            enum ACCESS accessibility)
{

    struct VariableEntry *newArgument = variable_entry_new(name->value, type, false, true, accessibility);

    if (scope_contains(scope, name->value))
    {
        log_tree(LOG_FATAL, name, "Redifinition of symbol %s!", name->value);
    }

    scope_insert(scope, name->value, newArgument, E_ARGUMENT, accessibility);

    return newArgument;
}

// create a new function accessible within the given scope
struct FunctionEntry *scope_create_function(struct Scope *parentScope,
                                            struct Ast *nameTree,
                                            struct Type *returnType,
                                            struct StructEntry *methodOf,
                                            enum ACCESS accessibility)
{
    struct FunctionEntry *newFunction = function_entry_new(parentScope, nameTree, returnType, methodOf);
    scope_insert(parentScope, nameTree->value, newFunction, E_FUNCTION, accessibility);
    return newFunction;
}

struct StructEntry *scope_create_struct(struct Scope *scope,
                                        char *name)
{
    struct StructEntry *wipStruct = malloc(sizeof(struct StructEntry));
    wipStruct->name = name;
    wipStruct->members = scope_new(scope, name, NULL, wipStruct);
    wipStruct->fieldLocations = stack_new();
    wipStruct->totalSize = 0;

    scope_insert(scope, name, wipStruct, E_STRUCT, A_PUBLIC);
    return wipStruct;
}

struct EnumEntry *scope_create_enum(struct Scope *scope,
                                    char *name)
{
    struct EnumEntry *wipEnum = malloc(sizeof(struct EnumEntry));
    wipEnum->name = name;
    wipEnum->parentScope = scope;
    wipEnum->members = set_new(enum_member_compare, free);
    wipEnum->unionSize = 0;

    scope_insert(scope, name, wipEnum, E_ENUM, A_PUBLIC);
    return wipEnum;
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

struct VariableEntry *scope_lookup_var_by_string(struct Scope *scope, char *name)
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

struct VariableEntry *scope_lookup_var(struct Scope *scope, struct Ast *name)
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

struct FunctionEntry *lookup_fun_by_string(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name);
    if (lookedUp == NULL)
    {
        InternalError("Lookup of undeclared function '%s'", name);
    }

    switch (lookedUp->type)
    {
    case E_FUNCTION:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up function!");
    }
}

struct FunctionEntry *scope_lookup_fun(struct Scope *scope, struct Ast *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, name, "Use of undeclared function '%s'", name->value);
    }
    switch (lookedUp->type)
    {
    case E_FUNCTION:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up function!");
    }
}

struct StructEntry *scope_lookup_struct(struct Scope *scope,
                                        struct Ast *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, name, "Use of undeclared struct '%s'", name->value);
    }
    switch (lookedUp->type)
    {
    case E_STRUCT:
        return lookedUp->entry;

    default:
        log_tree(LOG_FATAL, name, "%s is not a struct!", name->value);
    }

    return NULL;
}

struct StructEntry *scope_lookup_struct_by_type(struct Scope *scope,
                                                struct Type *type)
{
    if (type->basicType != VT_STRUCT || type->nonArray.complexType.name == NULL)
    {
        InternalError("Non-struct type or struct type with null name passed to lookupStructByType!");
    }

    struct ScopeMember *lookedUp = scope_lookup(scope, type->nonArray.complexType.name);
    if (lookedUp == NULL)
    {
        log(LOG_FATAL, "Use of undeclared struct '%s'", type->nonArray.complexType.name);
    }

    switch (lookedUp->type)
    {
    case E_STRUCT:
        return lookedUp->entry;

    default:
        InternalError("lookupStructByType for %s lookup got a non-struct ScopeMember!", type->nonArray.complexType.name);
    }
}

struct EnumEntry *scope_lookup_enum(struct Scope *scope,
                                    struct Ast *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, name, "Use of undeclared enum '%s'", name->value);
    }
    switch (lookedUp->type)
    {
    case E_ENUM:
        return lookedUp->entry;

    default:
        log_tree(LOG_FATAL, name, "%s is not an enum!", name->value);
    }

    return NULL;
}

struct EnumEntry *scope_lookup_enum_by_type(struct Scope *scope,
                                            struct Type *type)
{
    if (type->basicType != VT_ENUM || type->nonArray.complexType.name == NULL)
    {
        InternalError("Non-enum type or enum type with null name passed to lookupEnumByType!");
    }

    struct ScopeMember *lookedUp = scope_lookup(scope, type->nonArray.complexType.name);
    if (lookedUp == NULL)
    {
        log(LOG_FATAL, "Use of undeclared enum '%s'", type->nonArray.complexType.name);
    }

    switch (lookedUp->type)
    {
    case E_ENUM:
        return lookedUp->entry;

    default:
        InternalError("lookupEnumByType for %s lookup got a non-struct ScopeMember!", type->nonArray.complexType.name);
    }
}

struct EnumEntry *scope_lookup_enum_by_member_name(struct Scope *scope,
                                                   char *name)
{
    struct EnumMember dummyMember = {0};
    dummyMember.name = name;

    while (scope != NULL)
    {
        for (size_t memberIndex = 0; memberIndex < scope->entries->size; memberIndex++)
        {
            struct ScopeMember *member = (struct ScopeMember *)scope->entries->data[memberIndex];
            if (member->type == E_ENUM)
            {
                struct EnumEntry *scannedEnum = member->entry;
                if (set_find(scannedEnum->members, &dummyMember) != NULL)
                {
                    return scannedEnum;
                }
            }
        }
        scope = scope->parentScope;
    }

    return NULL;
}
