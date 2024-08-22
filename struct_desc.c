#include "struct_desc.h"

#include "log.h"
#include "symtab_basicblock.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include "symtab_type.h"
#include "util.h"

struct StructDesc *struct_desc_new(struct Scope *parentScope,
                                   char *name)
{
    struct StructDesc *wipStruct = malloc(sizeof(struct StructDesc));
    wipStruct->name = name;

    wipStruct->members = scope_new(parentScope, name, NULL, NULL);
    wipStruct->fieldLocations = deque_new(free);
    wipStruct->totalSize = 0;

    return wipStruct;
}

void struct_desc_free(struct StructDesc *theStruct)
{
    scope_free(theStruct->members);
    deque_free(theStruct->fieldLocations);

    free(theStruct);
}

struct StructDesc *struct_desc_clone(struct StructDesc *toClone, char *name)
{
    struct StructDesc *cloned = struct_desc_new(toClone->members->parentScope, name);

    Iterator *fieldIter = NULL;
    for (fieldIter = deque_front(toClone->fieldLocations); iterator_gettable(fieldIter); iterator_next(fieldIter))
    {
        struct StructField *field = iterator_get(fieldIter);

        // TODO: rework accessibility to just be sets for public/private members instead of the hokey "structs have scopes" thing
        struct ScopeMember *accessed = scope_lookup(cloned->members, field->variable->name, E_VARIABLE);

        struct_add_field(cloned, variable_entry_new(field->variable->name, &field->variable->type, field->variable->isGlobal, false, accessed->accessibility));
    }
    iterator_free(fieldIter);

    return cloned;
}

void struct_add_field(struct StructDesc *memberOf,
                      struct VariableEntry *variable)
{

    struct StructField *newMemberLocation = malloc(sizeof(struct StructField));

    newMemberLocation->offset = -1;
    newMemberLocation->variable = variable;

    deque_push_back(memberOf->fieldLocations, newMemberLocation);
}

void scope_resolve_capital_self(struct Scope *scope, struct TypeEntry *theType)
{
    Iterator *entryIter = NULL;
    for (entryIter = set_begin(scope->entries); iterator_gettable(entryIter); iterator_next(entryIter))
    {
        struct ScopeMember *member = iterator_get(entryIter);

        switch (member->type)
        {
        case E_VARIABLE:
        case E_ARGUMENT:
        {
            struct VariableEntry *variable = member->entry;
            type_try_resolve_vt_self(&variable->type, theType);
        }
        break;

        case E_FUNCTION:
        {
            struct FunctionEntry *function = member->entry;
            type_try_resolve_vt_self(&function->returnType, theType);
            scope_resolve_capital_self(function->mainScope, theType);
        }
        break;

        case E_TYPE:
        {
            struct TypeEntry *theType = member->entry;
            type_entry_resolve_capital_self(theType);
        }
        break;

        case E_SCOPE:
        {
            struct Scope *subScope = member->entry;
            scope_resolve_capital_self(subScope, theType);
        }
        break;

        case E_BASICBLOCK:
        {
            basic_block_resolve_capital_self(member->entry, theType);
        }
        break;

        case E_TRAIT:
            break;
        }
    }
    iterator_free(entryIter);
}

void type_entry_resolve_capital_self(struct TypeEntry *typeEntry)
{
    log(LOG_DEBUG, "Resolving capital self for type %s", typeEntry->name);
    switch (typeEntry->permutation)
    {
    case TP_PRIMITIVE:
        break;

    case TP_STRUCT:
        // type_entry_resolve_generics(typeEntry, typeEntry->generic.base.paramNames, typeEntry->generic.base.paramNames);
        struct_assign_offsets_to_fields(typeEntry->data.asStruct);
        break;

    case TP_ENUM:
        break;
    }
    Iterator *implIter = NULL;
    for (implIter = set_begin(typeEntry->implemented); iterator_gettable(implIter); iterator_next(implIter))
    {
        struct FunctionEntry *implemented = iterator_get(implIter);
        type_try_resolve_vt_self(&implemented->returnType, typeEntry);
        scope_resolve_capital_self(implemented->mainScope, typeEntry);
    }
    iterator_free(implIter);
}

void struct_assign_offsets_to_fields(struct StructDesc *theStruct)
{
    Iterator *fieldIter = NULL;
    for (fieldIter = deque_front(theStruct->fieldLocations); iterator_gettable(fieldIter); iterator_next(fieldIter))
    {
        struct StructField *handledField = iterator_get(fieldIter);
        // add the padding to the total size of the struct
        theStruct->totalSize += scope_compute_padding_for_alignment(theStruct->members, &handledField->variable->type, theStruct->totalSize);

        // place the new member at the (now aligned) current max size of the struct
        if (theStruct->totalSize > I64_MAX)
        {
            // TODO: implementation dependent size of size_t
            InternalError("Struct %s has size too large (%zd bytes)!", theStruct->name, theStruct->totalSize);
        }
        handledField->offset = (ssize_t)theStruct->totalSize;

        // add the size of the member we just added to the total size of the struct
        theStruct->totalSize += type_get_size(&handledField->variable->type, theStruct->members);
        log(LOG_DEBUG, "Assign offset %zu to member variable %s of struct %s - total struct size is now %zu", handledField->offset, handledField->variable->name, theStruct->name, theStruct->totalSize);
    }
    iterator_free(fieldIter);
}

// assuming we know that struct has a member with name identical to name->value, make sure we can actually access it
void struct_check_access(struct StructDesc *theStruct,
                         struct Ast *nameTree,
                         struct Scope *scope,
                         char *whatAccessingCalled)
{
    struct ScopeMember *accessed = scope_lookup(theStruct->members, nameTree->value, E_VARIABLE);
    if (accessed == NULL)
    {
        accessed = scope_lookup(theStruct->members, nameTree->value, E_FUNCTION);
    }

    switch (accessed->accessibility)
    {
    // nothing to check if public
    case A_PUBLIC:
        break;

    case A_PRIVATE:
        // check if the scope at which we are accessing is a subscope of (or identical to) the struct's scope
        do
        {
            if (scope == theStruct->members)
            {
                break;
            }
            scope = scope->parentScope;
        } while (scope != NULL);

        if (scope == NULL)
        {
            log_tree(LOG_FATAL, nameTree, "%s %s of struct %s has access specifier private - not accessible from this scope!", whatAccessingCalled, nameTree->value, theStruct->name);
        }
        break;
    }
}

// assuming we know that struct has a field with name identical to name, make sure we can actually access it
void struct_check_access_by_name(struct StructDesc *theStruct,
                                 char *name,
                                 struct Scope *scope,
                                 char *whatAccessingCalled)
{
    struct ScopeMember *accessed = scope_lookup(theStruct->members, name, E_VARIABLE);

    switch (accessed->accessibility)
    {
    // nothing to check if public
    case A_PUBLIC:
        break;

    case A_PRIVATE:
    {
        struct Scope *checkedScope = scope;
        // check if the scope at which we are accessing is a subscope of (or identical to) the struct's scope
        do
        {
            if (checkedScope == theStruct->members)
            {
                break;
            }
            checkedScope = checkedScope->parentScope;
        } while (checkedScope != NULL);

        if (checkedScope == NULL)
        {
            log(LOG_FATAL, "%s %s of struct %s has access specifier private - not accessible from this scope!", whatAccessingCalled, name, theStruct->name);
        }
        break;
    }
    }
}

struct StructField *struct_lookup_field(struct StructDesc *theStruct,
                                        struct Ast *nameTree,
                                        struct Scope *scope)
{
    if (nameTree->type != T_IDENTIFIER)
    {
        log_tree(LOG_FATAL,
                 nameTree,
                 "Non-identifier tree %s (%s) passed to Struct_lookupOffsetOfMemberVariable!\n",
                 nameTree->value,
                 token_get_name(nameTree->type));
    }

    struct StructField *returnedField = NULL;
    Iterator *fieldIterator = NULL;
    for (fieldIterator = deque_front(theStruct->fieldLocations); iterator_gettable(fieldIterator); iterator_next(fieldIterator))
    {
        struct StructField *field = iterator_get(fieldIterator);
        if (!strcmp(field->variable->name, nameTree->value))
        {
            returnedField = field;
            break;
        }
    }
    iterator_free(fieldIterator);

    if (returnedField == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of nonexistent field \"%s\" in struct %s", nameTree->value, theStruct->name);
    }
    else
    {
        struct_check_access(theStruct, nameTree, scope, "Field");
    }

    return returnedField;
}

struct StructField *struct_lookup_field_by_name(struct StructDesc *theStruct,
                                                char *name,
                                                struct Scope *scope)
{
    struct StructField *returnedField = NULL;
    Iterator *fieldIterator = NULL;
    for (fieldIterator = deque_front(theStruct->fieldLocations); iterator_gettable(fieldIterator); iterator_next(fieldIterator))
    {
        struct StructField *field = iterator_get(fieldIterator);
        if (!strcmp(field->variable->name, name))
        {
            returnedField = field;
            break;
        }
    }
    iterator_free(fieldIterator);

    if (returnedField == NULL)
    {
        log(LOG_FATAL, "Use of nonexistent field %s in struct %s", name, theStruct->name);
    }
    else
    {
        struct_check_access_by_name(theStruct, name, scope, "Field");
    }

    return returnedField;
}

struct FunctionEntry *struct_looup_method(struct StructDesc *theStruct,
                                          struct Ast *name,
                                          struct Scope *scope)
{
    struct FunctionEntry *returnedMethod = NULL;

    struct ScopeMember *lookedUpEntry = scope_lookup_no_parent(theStruct->members, name->value, E_FUNCTION);

    if (lookedUpEntry == NULL)
    {
        log_tree(LOG_FATAL, name, "Attempt to call nonexistent method %s.%s\n", theStruct->name, name->value);
    }

    if (lookedUpEntry->type != E_FUNCTION)
    {
        log_tree(LOG_FATAL, name, "Attempt to call non-method member %s.%s as method!\n", theStruct->name, name->value);
    }

    returnedMethod = lookedUpEntry->entry;

    struct_check_access(theStruct, name, scope, "Method");

    if (!returnedMethod->isMethod)
    {
        log_tree(LOG_FATAL, name, "Attempt to call non-member associated function %s::%s as a method!\n", theStruct->name, name->value);
    }

    return returnedMethod;
}

struct FunctionEntry *struct_lookup_method_by_string(struct StructDesc *theStruct,
                                                     char *name)
{
    struct FunctionEntry *returnedMethod = NULL;

    struct ScopeMember *lookedUpEntry = scope_lookup_no_parent(theStruct->members, name, E_FUNCTION);

    if (lookedUpEntry == NULL)
    {
        InternalError("Attempt to call nonexistent method %s.%s\n", theStruct->name, name);
    }

    if (lookedUpEntry->type != E_FUNCTION)
    {
        InternalError("Attempt to call non-method member %s.%s as method!\n", theStruct->name, name);
    }

    returnedMethod = lookedUpEntry->entry;

    if (!returnedMethod->isMethod)
    {
        InternalError("Attempt to call non-member associated function %s::%s as a method!\n", theStruct->name, name);
    }

    return returnedMethod;
}

struct FunctionEntry *struct_lookup_associated_function_by_string(struct StructDesc *theStruct,
                                                                  char *name)
{
    struct FunctionEntry *returendAssociated = NULL;

    struct ScopeMember *lookedUpEntry = scope_lookup_no_parent(theStruct->members, name, E_FUNCTION);

    if (lookedUpEntry == NULL)
    {
        log(LOG_FATAL, "Attempt to call nonexistent associated function %s::%s\n", theStruct->name, name);
    }

    if (lookedUpEntry->type != E_FUNCTION)
    {
        log(LOG_FATAL, "Attempt to call non-function member %s.%s as an associated function!\n", theStruct->name, name);
    }

    returendAssociated = lookedUpEntry->entry;

    if (returendAssociated->isMethod)
    {
        log(LOG_FATAL, "Attempt to call method %s.%s as an associated function!\n", theStruct->name, name);
    }

    return returendAssociated;
}

void struct_desc_print(struct StructDesc *theStruct, size_t depth, FILE *outFile)
{
    Iterator *fieldIter = NULL;
    for (fieldIter = deque_front(theStruct->fieldLocations); iterator_gettable(fieldIter); iterator_next(fieldIter))
    {
        struct StructField *field = iterator_get(fieldIter);
        for (size_t i = 0; i < depth; i++)
        {
            fprintf(outFile, "\t");
        }
        fprintf(outFile, "Field %s @ offset %zd\n", field->variable->name, field->offset);
    }
    iterator_free(fieldIter);
}

void struct_desc_resolve_generics(struct StructDesc *theStruct, HashTable *paramsMap, char *name, List *params)
{
    InternalError("struct_desc_resolve_generics not implemnted yet!");
}
