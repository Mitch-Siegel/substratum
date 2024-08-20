#include "symtab_scope.h"

#include "log.h"
#include "symtab_basicblock.h"
#include "symtab_enum.h"
#include "symtab_function.h"
#include "symtab_struct.h"
#include "symtab_trait.h"
#include "symtab_type.h"
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

    case E_TYPE:
        type_entry_free(member->entry);

    case E_BASICBLOCK:
        basic_block_free(member->entry);
        break;

    case E_TRAIT:
        trait_free(member->entry);
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
                                        char *name)
{
    struct TypeEntry *wipType = type_entry_new_struct(name, scope, G_NONE, NULL);
    scope_insert(scope, name, wipType, E_TYPE, A_PUBLIC);
    struct StructEntry *wipStruct = wipType->data.asStruct;
    return wipStruct;
}

struct StructEntry *scope_create_generic_base_struct(struct Scope *scope,
                                                     char *name,
                                                     List *paramNames)
{
    struct TypeEntry *wipType = type_entry_new_struct(name, scope, G_BASE, paramNames);
    scope_insert(scope, name, wipType, E_TYPE, A_PUBLIC);

    return wipType->data.asStruct;
}

// TODO: enum_entry_new()
struct EnumEntry *scope_create_enum(struct Scope *scope,
                                    char *name)
{
    struct TypeEntry *wipType = type_entry_new_enum(name, scope);
    scope_insert(scope, name, wipType, E_TYPE, A_PUBLIC);

    return wipType->data.asEnum;
}

struct TraitEntry *scope_create_trait(struct Scope *scope,
                                      char *name)
{
    struct TraitEntry *newTrait = trait_new(name, scope);
    scope_insert(scope, name, newTrait, E_TRAIT, A_PUBLIC);
    return newTrait;
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
    struct ScopeMember *lookedUp = scope_lookup(scope, nameTree->value, E_TYPE);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared struct '%s'", nameTree->value);
    }

    switch (lookedUp->type)
    {
    case E_TYPE:
        struct TypeEntry *lookedUpType = lookedUp->entry;
        switch (lookedUpType->permutation)
        {
        case TP_PRIMITIVE:
            log_tree(LOG_FATAL, nameTree, "%s is a primitive type, not a struct!", nameTree->value);
            break;

        case TP_STRUCT:
            return lookedUpType->data.asStruct;

        case TP_ENUM:
            log_tree(LOG_FATAL, nameTree, "%s is an enum type, not a struct!", nameTree->value);
            break;
        }

    default:
        log_tree(LOG_FATAL, nameTree, "%s is not a struct!", nameTree->value);
    }

    return NULL;
}

struct StructEntry *scope_lookup_struct_by_type(struct Scope *scope,
                                                struct Type *type)
{
    if (type->basicType == VT_SELF)
    {
        if (scope->parentStruct == NULL)
        {
            InternalError("Use of 'Self' outside of struct context!");
        }

        return scope->parentStruct;
    }

    if (type->basicType != VT_STRUCT || type->nonArray.complexType.name == NULL)
    {
        InternalError("Non-struct type or struct type with null name passed to lookupStructByType!");
    }

    struct StructEntry *lookedUpStruct = scope_lookup_struct_by_name(scope, type->nonArray.complexType.name);

    switch (lookedUpStruct->genericType)
    {
    case G_NONE:
        return lookedUpStruct;

    case G_BASE:
    {
        if (type->nonArray.complexType.genericParams != NULL)
        {
            lookedUpStruct = struct_get_or_create_generic_instantiation(lookedUpStruct, type->nonArray.complexType.genericParams);
        }
        return lookedUpStruct;
    }
    break;

    case G_INSTANCE:
        if (type->nonArray.complexType.genericParams != NULL)
        {
            InternalError("Non-generic struct type %s used with generic parameters!", type->nonArray.complexType.name);
        }
        struct StructEntry *instantiatedStruct = struct_get_or_create_generic_instantiation(lookedUpStruct, type->nonArray.complexType.genericParams);
        return instantiatedStruct;
    }

    return NULL;
}

struct StructEntry *scope_lookup_struct_by_name(struct Scope *scope,
                                                char *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name, E_TYPE);
    if (lookedUp == NULL)
    {
        InternalError("Use of undeclared struct '%s'", name);
    }

    struct TypeEntry *lookedUpType = lookedUp->entry;

    switch (lookedUpType->permutation)
    {
    case TP_PRIMITIVE:
        InternalError("%s names a primitive type, not a struct", name);
        break;

    case TP_STRUCT:
        return lookedUpType->data.asStruct;

    case TP_ENUM:
        InternalError("%s names an enum type, not a struct", name);
        break;
    }

    return NULL;
}

struct EnumEntry *scope_lookup_enum(struct Scope *scope,
                                    struct Ast *nameTree)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, nameTree->value, E_TYPE);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared enum '%s'", nameTree->value);
    }

    struct TypeEntry *lookedUpType = lookedUp->entry;

    switch (lookedUpType->permutation)
    {
    case TP_PRIMITIVE:
        log_tree(LOG_FATAL, nameTree, "%s names a primitive type, not an enum", nameTree->value);
        break;

    case TP_STRUCT:
        log_tree(LOG_FATAL, nameTree, "%s names a struct type, not an enum", nameTree->value);
        break;

    case TP_ENUM:
        return lookedUpType->data.asEnum;
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

    struct ScopeMember *lookedUp = scope_lookup(scope, type->nonArray.complexType.name, E_TYPE);
    if (lookedUp == NULL)
    {
        log(LOG_FATAL, "Use of undeclared enum '%s'", type->nonArray.complexType.name);
    }

    struct TypeEntry *lookedUpType = lookedUp->entry;

    switch (lookedUpType->permutation)
    {
    case TP_PRIMITIVE:
        InternalError("%s names a primitive type, not a struct", type->nonArray.complexType.name);

    case TP_STRUCT:
        InternalError("%s names a struct type, not a struct", type->nonArray.complexType.name);

    case TP_ENUM:
        return lookedUpType->data.asEnum;
    }

    return NULL;
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
            if (member->type == E_TYPE)
            {
                struct TypeEntry *memberType = member->entry;
                if (memberType->permutation == TP_ENUM)
                {
                    struct EnumEntry *scannedEnum = memberType->data.asEnum;
                    if (set_find(scannedEnum->members, &dummyMember) != NULL)
                    {
                        iterator_free(memberIterator);
                        return scannedEnum;
                    }
                }
            }
        }
        iterator_free(memberIterator);
        scope = scope->parentScope;
    }

    return NULL;
}

struct TraitEntry *scope_lookup_trait(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name, E_TRAIT);
    if (lookedUp == NULL)
    {
        InternalError("Use of undeclared trait '%s'", name);
    }

    switch (lookedUp->type)
    {
    case E_TRAIT:
        return lookedUp->entry;

    default:
        InternalError("lookupTrait for %s lookup got a non-trait ScopeMember!", name);
    }
}

struct BasicBlock *scope_lookup_block_by_number(struct Scope *scope, size_t label)
{
    char blockName[32];
    sprintf(blockName, "Block%zu", label);
    struct ScopeMember *blockMember = scope_lookup(scope, blockName, E_BASICBLOCK);
    if (blockMember == NULL)
    {
        return NULL;
    }
    return blockMember->entry;
}

// TODO: better param names than toClone and cloneTo. seriously.
struct FunctionEntry *function_entry_clone(struct FunctionEntry *toClone, struct Scope *cloneTo)
{
    log(LOG_DEBUG, "function_entry_clone: %s", toClone->name);
    struct FunctionEntry *cloned = function_entry_new(cloneTo, &toClone->correspondingTree, cloneTo->parentStruct);
    cloned->returnType = type_duplicate_non_pointer(&toClone->returnType);
    cloned->callsOtherFunction = toClone->callsOtherFunction;
    cloned->isAsmFun = toClone->isAsmFun;
    cloned->isDefined = toClone->isDefined;
    cloned->isMethod = toClone->isMethod;

    scope_clone_to(cloned->mainScope, toClone->mainScope);

    log(LOG_DEBUG, "initialize arguments list for clone of function %s", toClone->name);
    Iterator *argIter = NULL;
    for (argIter = deque_front(toClone->arguments); iterator_gettable(argIter); iterator_next(argIter))
    {
        struct VariableEntry *oldArg = iterator_get(argIter);
        struct VariableEntry *newArg = scope_lookup_var_by_string(cloned->mainScope, oldArg->name);
        if (newArg == NULL)
        {
            InternalError("Couldn't find expected argument %s when cloning function %s", oldArg->name, toClone->name);
        }
        log(LOG_DEBUG, "%s", newArg->name);
        deque_push_back(cloned->arguments, newArg);
    }
    iterator_free(argIter);

    log(LOG_DEBUG, "initialize basic block list for clone of function %s", toClone->name);
    Iterator *blockIter = NULL;
    for (blockIter = list_begin(toClone->BasicBlockList); iterator_gettable(blockIter); iterator_next(blockIter))
    {
        struct BasicBlock *oldBlock = iterator_get(blockIter);
        struct BasicBlock *newBlock = scope_lookup_block_by_number(cloned->mainScope, oldBlock->labelNum);
        if (newBlock == NULL)
        {
            InternalError("Couldn't find expected basic block %zu when cloning function %s", oldBlock->labelNum, toClone->name);
        }
        log(LOG_DEBUG, "%zu", newBlock->labelNum);

        list_append(cloned->BasicBlockList, newBlock);
    }
    iterator_free(blockIter);

    return cloned;
}

struct BasicBlock *basic_block_clone(struct BasicBlock *toClone, struct Scope *clonedTo)
{
    struct BasicBlock *clone = basic_block_new(toClone->labelNum);

    Iterator *tacRunner = NULL;
    for (tacRunner = list_begin(toClone->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
    {
        struct TACLine *lineToClone = iterator_get(tacRunner);

        // TODO: tac_line_duplicate
        struct TACLine *clonedLine = new_tac_line(lineToClone->operation, &lineToClone->correspondingTree);
        memcpy(clonedLine, lineToClone, sizeof(struct TACLine));
        clonedLine->allocFile = __FILE__;
        clonedLine->allocLine = __LINE__;

        struct OperandUsages operandUsages = get_operand_usages(clonedLine);
        while (operandUsages.reads->size > 0)
        {
            struct TACOperand *readOperand = deque_pop_front(operandUsages.reads);
            switch (readOperand->permutation)
            {
            case VP_STANDARD:
            case VP_TEMP:
                readOperand->name.variable = scope_lookup_var_by_string(clonedTo, readOperand->name.variable->name);
                break;

            default:
                break;
            }
        }

        while (operandUsages.writes->size > 0)
        {
            struct TACOperand *writtenOperand = deque_pop_front(operandUsages.writes);
            switch (writtenOperand->permutation)
            {
            case VP_STANDARD:
            case VP_TEMP:
                writtenOperand->name.variable = scope_lookup_var_by_string(clonedTo, writtenOperand->name.variable->name);
                break;

            default:
                break;
            }
        }

        deque_free(operandUsages.reads);
        deque_free(operandUsages.writes);

        size_t tacIndex = lineToClone->index;
        basic_block_append(clone, clonedLine, &tacIndex);
    }
    iterator_free(tacRunner);

    return clone;
}

void scope_clone_to(struct Scope *clonedTo, struct Scope *toClone)
{
    log(LOG_DEBUG, "scope_clone_to %s<-%s", clonedTo->name, toClone->name);
    Iterator *memberIterator = NULL;
    for (memberIterator = set_begin(toClone->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
    {
        struct ScopeMember *memberToClone = iterator_get(memberIterator);
        void *entry = NULL;
        switch (memberToClone->type)
        {
        case E_VARIABLE:
        {
            struct VariableEntry *variableToClone = memberToClone->entry;
            struct Type dupType = type_duplicate_non_pointer(&variableToClone->type);
            entry = variable_entry_new(variableToClone->name, &dupType, variableToClone->isGlobal, false, memberToClone->accessibility);
        }
        break;

        case E_ARGUMENT:
        {
            struct VariableEntry *argumentToClone = memberToClone->entry;
            struct Type dupType = type_duplicate_non_pointer(&argumentToClone->type);
            entry = variable_entry_new(argumentToClone->name, &dupType, argumentToClone->isGlobal, true, memberToClone->accessibility);
        }
        break;

        case E_SCOPE:
        {
            struct Scope *clonedScope = memberToClone->entry;
            entry = scope_new(clonedTo, clonedScope->name, clonedTo->parentFunction, clonedTo->parentStruct);
            scope_clone_to(entry, clonedScope);
        }
        break;

        case E_BASICBLOCK:
        case E_FUNCTION:
        case E_TYPE:
        case E_TRAIT:
            break;
        }

        if (entry != NULL)
        {
            scope_insert(clonedTo, memberToClone->name, entry, memberToClone->type, memberToClone->accessibility);
        }
    }
    iterator_free(memberIterator);

    for (memberIterator = set_begin(toClone->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
    {
        struct ScopeMember *memberToClone = iterator_get(memberIterator);
        void *entry = NULL;
        switch (memberToClone->type)
        {
        case E_FUNCTION:
        {
            struct FunctionEntry *clonedFunction = memberToClone->entry;
            entry = function_entry_clone(clonedFunction, clonedTo);
        }
        break;

        case E_TYPE:
            InternalError("scope_clone on types not yet implemented!");

        case E_BASICBLOCK:
        {
            struct BasicBlock *clonedBlock = memberToClone->entry;
            entry = basic_block_clone(clonedBlock, clonedTo);
        }
        break;

        case E_VARIABLE:
        case E_ARGUMENT:
        case E_SCOPE:
        case E_TRAIT:
            break;
        }

        if (entry != NULL)
        {
            scope_insert(clonedTo, memberToClone->name, entry, memberToClone->type, memberToClone->accessibility);
        }
    }
    iterator_free(memberIterator);
    // case E_FUNCTION:
    // case E_STRUCT:
    // case E_ENUM:

    // {
    //
    // }
    // break;
}

void try_resolve_generic_for_type(struct Type *type, HashTable *paramsMap, char *resolvedStructName, List *resolvedParams)
{
    char *typeName = type_get_name(type);
    free(typeName);

    if (type->basicType == VT_GENERIC_PARAM)
    {
        struct Type *resolvedToType = hash_table_find(paramsMap, type->nonArray.complexType.name);
        if (resolvedToType == NULL)
        {
            InternalError("Couldn't resolve actual type for generic parameter of name %s", type_get_name(type));
        }
        *type = *resolvedToType;
    }
    else if (type->basicType == VT_ARRAY)
    {
        try_resolve_generic_for_type(type->array.type, paramsMap, resolvedStructName, resolvedParams);
    }
    else if ((type->basicType == VT_STRUCT) && (!strcmp(type->nonArray.complexType.name, resolvedStructName)))
    {
        type->nonArray.complexType.genericParams = resolvedParams;
    }
}

void basic_block_resolve_generics(struct BasicBlock *block, HashTable *paramsMap, char *resolvedStructName, List *resolvedParams)
{
    Iterator *tacRunner = NULL;
    for (tacRunner = list_begin(block->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
    {
        struct TACLine *resolvedLine = iterator_get(tacRunner);
        struct OperandUsages operandUsages = get_operand_usages(resolvedLine);
        while (operandUsages.reads->size > 0)
        {
            struct TACOperand *readOperand = deque_pop_front(operandUsages.reads);
            try_resolve_generic_for_type(&readOperand->castAsType, paramsMap, resolvedStructName, resolvedParams);
        }

        while (operandUsages.writes->size > 0)
        {
            struct TACOperand *writtenOperand = deque_pop_front(operandUsages.writes);
            try_resolve_generic_for_type(&writtenOperand->castAsType, paramsMap, resolvedStructName, resolvedParams);
        }

        deque_free(operandUsages.reads);
        deque_free(operandUsages.writes);
    }
    iterator_free(tacRunner);
}

void scope_resolve_generics(struct Scope *scope, HashTable *paramsMap, char *resolvedStructName, List *resolvedParams)
{
    Iterator *memberIterator = NULL;
    for (memberIterator = set_begin(scope->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
    {
        struct ScopeMember *memberToResolve = iterator_get(memberIterator);
        switch (memberToResolve->type)
        {
        case E_VARIABLE:
        case E_ARGUMENT:
        {
            struct VariableEntry *resolved = memberToResolve->entry;
            try_resolve_generic_for_type(&resolved->type, paramsMap, resolvedStructName, resolvedParams);
        }
        break;

        case E_FUNCTION:
        {
            struct FunctionEntry *resolved = memberToResolve->entry;
            try_resolve_generic_for_type(&resolved->returnType, paramsMap, resolvedStructName, resolvedParams);
            scope_resolve_generics(resolved->mainScope, paramsMap, resolvedStructName, resolvedParams);
        }
        break;

        case E_TYPE:
        {
            InternalError("Recursive generic resolution for sub-types not implemented yet!");
        }
        break;

        case E_SCOPE:
        {
            scope_resolve_generics(memberToResolve->entry, paramsMap, resolvedStructName, resolvedParams);
        }
        break;

        case E_BASICBLOCK:
        {
            struct BasicBlock *resolvedBlock = memberToResolve->entry;
            basic_block_resolve_generics(resolvedBlock, paramsMap, resolvedStructName, resolvedParams);
        }
        break;

        case E_TRAIT:
        {
            InternalError("Recursive generic resolution for sub-traits not implemented yet!");
        }
        break;
        }
    }
    iterator_free(memberIterator);
}
