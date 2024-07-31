#include "symtab_scope.h"

#include "log.h"
#include "symtab_basicblock.h"
#include "symtab_enum.h"
#include "symtab_function.h"
#include "symtab_struct.h"
#include "symtab_variable.h"
#include "util.h"

extern struct Dictionary *parseDict;

ssize_t scope_member_compare(struct ScopeMember *memberA, struct ScopeMember *memberB)
{
    ssize_t cmpVal = 0;
    if (memberA->type == memberB->type)
    {
        cmpVal = strcmp(memberA->name, memberB->name);
    }
    else
    {
        cmpVal = (ssize_t)memberA->type - (ssize_t)memberB->type;
    }
    return cmpVal;
}

void scope_member_free(struct ScopeMember *member)
{
    switch (member->type)
    {
    case E_SCOPE:
        scope_free(member->entry);
        break;

    case E_FUNCTION:
        function_entry_free(member->entry);
        break;

    case E_VARIABLE:
    case E_ARGUMENT:
        variable_entry_free(member->entry);
        break;

    case E_STRUCT:
        struct_entry_free(member->entry);
        break;

    case E_ENUM:
        enum_entry_free(member->entry);
        break;

    case E_BASICBLOCK:
        basic_block_free(member->entry);
        break;
    }
    free(member);
}

/*
 * Scope functions
 *
 */
struct Scope *scope_new(struct Scope *parentScope, char *name, struct FunctionEntry *parentFunction, struct StructEntry *parentImpl)
{
    struct Scope *wip = malloc(sizeof(struct Scope));
    wip->entries = set_new((void (*)(void *))scope_member_free, (ssize_t(*)(void *, void *))scope_member_compare);

    wip->parentScope = parentScope;
    wip->parentFunction = parentFunction;
    wip->parentStruct = parentImpl;
    wip->name = name;
    wip->subScopeCount = 0;
    return wip;
}

void scope_free(struct Scope *scope)
{
    set_free(scope->entries);
    free(scope);
}

// insert a member with a given name and pointer to entry, along with info about the entry type
void scope_insert(struct Scope *scope, char *name, void *newEntry, enum SCOPE_MEMBER_TYPE type, enum ACCESS accessibility)
{
    if (scope_contains(scope, name, type))
    {
        InternalError("Error defining symbol [%s] - name already exists!", name);
    }
    struct ScopeMember *wipMember = malloc(sizeof(struct ScopeMember));
    wipMember->name = name;
    wipMember->entry = newEntry;
    wipMember->type = type;
    wipMember->accessibility = accessibility;
    set_insert(scope->entries, wipMember);
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

    struct Scope *newScope = scope_new(parent_scope, newScopeName, parent_scope->parentFunction, parent_scope->parentStruct);

    scope_insert(parent_scope, newScopeName, newScope, E_SCOPE, A_PUBLIC);
    return newScope;
}

// create a variable within the given scope
struct VariableEntry *scope_create_variable(struct Scope *scope,
                                            struct Ast *nameTree,
                                            struct Type *type,
                                            bool isGlobal,
                                            enum ACCESS accessibility)
{
    if (scope_contains(scope, nameTree->value, E_VARIABLE) || scope_contains(scope, nameTree->value, E_ARGUMENT))
    {
        log_tree(LOG_FATAL, nameTree, "Redifinition of symbol %s!", nameTree->value);
    }

    return scope_create_variable_by_name(scope, nameTree->value, type, isGlobal, accessibility);
}

// create a variable within the given scope
struct VariableEntry *scope_create_variable_by_name(struct Scope *scope,
                                                    char *name,
                                                    struct Type *type,
                                                    bool isGlobal,
                                                    enum ACCESS accessibility)
{
    if (scope_contains(scope, name, E_VARIABLE) || scope_contains(scope, name, E_ARGUMENT))
    {
        InternalError("Redifinition of symbol %s!", name);
    }

    struct VariableEntry *newVariable = variable_entry_new(name, type, isGlobal, false, accessibility);

    scope_insert(scope, name, newVariable, E_VARIABLE, accessibility);

    return newVariable;
}

// create an argument within the given scope
struct VariableEntry *scope_create_argument(struct Scope *scope,
                                            struct Ast *name,
                                            struct Type *type,
                                            enum ACCESS accessibility)
{

    struct VariableEntry *newArgument = variable_entry_new(name->value, type, false, true, accessibility);

    if (scope_contains(scope, name->value, E_VARIABLE) || scope_contains(scope, name->value, E_ARGUMENT))
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
    struct FunctionEntry *newFunction = function_entry_new(parentScope, nameTree, methodOf);
    newFunction->returnType = *returnType;
    scope_insert(parentScope, nameTree->value, newFunction, E_FUNCTION, accessibility);
    return newFunction;
}

struct StructEntry *scope_create_struct(struct Scope *scope,
                                        char *name,
                                        List *genericParams)
{
    struct StructEntry *wipStruct = malloc(sizeof(struct StructEntry));
    wipStruct->name = name;
    wipStruct->genericParameters = genericParams;
    wipStruct->members = scope_new(scope, name, NULL, wipStruct);
    wipStruct->fieldLocations = stack_new(free);
    wipStruct->totalSize = 0;

    scope_insert(scope, name, wipStruct, E_STRUCT, A_PUBLIC);
    return wipStruct;
}

// TODO: enum_entry_new()
struct EnumEntry *scope_create_enum(struct Scope *scope,
                                    char *name)
{
    struct EnumEntry *wipEnum = malloc(sizeof(struct EnumEntry));
    wipEnum->name = name;
    wipEnum->parentScope = scope;
    wipEnum->members = set_new(free, (ssize_t(*)(void *, void *))enum_member_compare);
    wipEnum->unionSize = 0;

    scope_insert(scope, name, wipEnum, E_ENUM, A_PUBLIC);
    return wipEnum;
}

// Scope lookup functions

bool scope_contains(struct Scope *scope, char *name, enum SCOPE_MEMBER_TYPE type)
{
    struct ScopeMember dummyMember = {0};
    dummyMember.name = name;
    dummyMember.type = type;
    return (set_find(scope->entries, &dummyMember) != NULL);
}

struct ScopeMember *scope_lookup_no_parent(struct Scope *scope, char *name, enum SCOPE_MEMBER_TYPE type)
{
    struct ScopeMember dummyMember = {0};
    dummyMember.name = name;
    dummyMember.type = type;
    return set_find(scope->entries, &dummyMember);
}

// if a member with the given name exists in this scope or any of its parents, return it
// also looks up entries from deeper scopes, but only as their mangled names specify
struct ScopeMember *scope_lookup(struct Scope *scope, char *name, enum SCOPE_MEMBER_TYPE type)
{
    while (scope != NULL)
    {
        struct ScopeMember *foundThisScope = scope_lookup_no_parent(scope, name, type);
        if (foundThisScope != NULL)
        {
            return foundThisScope;
        }
        scope = scope->parentScope;
    }
    return NULL;
}

struct VariableEntry *scope_lookup_var_by_string(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUpVar = scope_lookup(scope, name, E_VARIABLE);
    struct ScopeMember *lookedUpArg = scope_lookup(scope, name, E_ARGUMENT);
    if ((lookedUpVar == NULL) && (lookedUpArg == NULL))
    {
        return NULL;
    }

    struct ScopeMember *lookedUp = (lookedUpVar != NULL) ? lookedUpVar : lookedUpArg;

    switch (lookedUp->type)
    {
    case E_ARGUMENT:
    case E_VARIABLE:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up variable [%s]!", name);
    }

    return NULL;
}

struct VariableEntry *scope_lookup_var(struct Scope *scope, struct Ast *nameTree)
{
    struct VariableEntry *lookedUp = scope_lookup_var_by_string(scope, nameTree->value);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared variable '%s'", nameTree->value);
    }

    return lookedUp;
}

struct FunctionEntry *lookup_fun_by_string(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name, E_FUNCTION);
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

struct FunctionEntry *scope_lookup_fun(struct Scope *scope, struct Ast *nameTree)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, nameTree->value, E_FUNCTION);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared function '%s'", nameTree->value);
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
                                        struct Ast *nameTree)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, nameTree->value, E_STRUCT);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared struct '%s'", nameTree->value);
    }
    switch (lookedUp->type)
    {
    case E_STRUCT:
        return lookedUp->entry;

    default:
        log_tree(LOG_FATAL, nameTree, "%s is not a struct!", nameTree->value);
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

    struct ScopeMember *lookedUp = scope_lookup(scope, type->nonArray.complexType.name, E_STRUCT);
    if (lookedUp == NULL)
    {
        InternalError("Use of undeclared struct '%s'", type->nonArray.complexType.name);
    }

    switch (lookedUp->type)
    {
    case E_STRUCT:
        return lookedUp->entry;

    default:
        InternalError("lookupStructByType for %s lookup got a non-struct ScopeMember!", type->nonArray.complexType.name);
    }
}

struct StructEntry *scope_lookup_struct_by_name(struct Scope *scope,
                                                char *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name, E_STRUCT);
    if (lookedUp == NULL)
    {
        InternalError("Use of undeclared struct '%s'", name);
    }

    switch (lookedUp->type)
    {
    case E_STRUCT:
        return lookedUp->entry;

    default:
        InternalError("lookupStructByType for %s lookup got a non-struct ScopeMember!", name);
    }
    return NULL;
}

struct EnumEntry *scope_lookup_enum(struct Scope *scope,
                                    struct Ast *nameTree)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, nameTree->value, E_ENUM);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared enum '%s'", nameTree->value);
    }
    switch (lookedUp->type)
    {
    case E_ENUM:
        return lookedUp->entry;

    default:
        log_tree(LOG_FATAL, nameTree, "%s is not an enum!", nameTree->value);
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

    struct ScopeMember *lookedUp = scope_lookup(scope, type->nonArray.complexType.name, E_ENUM);
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
        Iterator *memberIterator = NULL;
        for (memberIterator = set_begin(scope->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
        {
            struct ScopeMember *member = iterator_get(memberIterator);
            if (member->type == E_ENUM)
            {
                struct EnumEntry *scannedEnum = member->entry;
                if (set_find(scannedEnum->members, &dummyMember) != NULL)
                {
                    iterator_free(memberIterator);
                    return scannedEnum;
                }
            }
        }
        iterator_free(memberIterator);
        scope = scope->parentScope;
    }

    return NULL;
}
