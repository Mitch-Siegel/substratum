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

    wipStruct->members = scope_new(parentScope, name, NULL);
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
        struct ScopeMember *accessed = scope_lookup(toClone->members, field->variable->name, E_VARIABLE);

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
    log(LOG_DEBUG, "Resolving capital self for scope %s", scope->name);
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
            log(LOG_DEBUG, "Resolving capital self for function %s", member->name);
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
    char *typeName = type_entry_name(typeEntry);
    log(LOG_DEBUG, "Resolving capital self for type %s", typeName);
    free(typeName);

    scope_resolve_capital_self(typeEntry->implemented, typeEntry);
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

ssize_t struct_desc_compare(struct StructDesc *a, struct StructDesc *b)
{
    ssize_t diff = strcmp(a->name, b->name);
    if (diff != 0)
    {
        return diff;
    }

    diff = a->fieldLocations->size - b->fieldLocations->size;
    return diff;
}

// assuming we know that struct has a member with name identical to name->value, make sure we can actually access it
void struct_check_field_access(struct StructDesc *theStruct,
                               struct Ast *nameTree,
                               struct Scope *accessedFromScope,
                               char *whatAccessingCalled)
{
    // if the scope from which we are accessing:
    // 1. is a function scope
    // 2. the function is implemented for the struct
    // 3. the struct is the same as the one we are accessing
    // always allow access because private access is allowed
    if ((accessedFromScope->parentFunction->implementedFor != NULL) &&
        (accessedFromScope->parentFunction->implementedFor->permutation == TP_STRUCT) &&
        (struct_desc_compare(theStruct, accessedFromScope->parentFunction->implementedFor->data.asStruct) == 0))
    {
        return;
    }

    struct ScopeMember *accessed = scope_lookup(theStruct->members, nameTree->value, E_VARIABLE);

    switch (accessed->accessibility)
    {
    // nothing to check if public
    case A_PUBLIC:
        break;

    case A_PRIVATE:
        // check if the scope at which we are accessing is a subscope of (or identical to) the struct's scope
        do
        {
            if (accessedFromScope == theStruct->members)
            {
                break;
            }
            accessedFromScope = accessedFromScope->parentScope;
        } while (accessedFromScope != NULL);

        if (accessedFromScope == NULL)
        {
            log_tree(LOG_FATAL, nameTree, "%s %s of struct %s has access specifier private - not accessible from this scope!", whatAccessingCalled, nameTree->value, theStruct->name);
        }
        break;
    }
}

// assuming we know that struct has a field with name identical to name, make sure we can actually access it
void struct_check_access_by_name(struct StructDesc *theStruct,
                                 char *name,
                                 struct Scope *accessedFromScope,
                                 char *whatAccessingCalled)
{
    // if the scope from which we are accessing:
    // 1. is a function scope
    // 2. the function is implemented for the struct
    // 3. the struct is the same as the one we are accessing
    // always allow access because private access is allowed
    if ((accessedFromScope->parentFunction->implementedFor != NULL) &&
        (accessedFromScope->parentFunction->implementedFor->permutation == TP_STRUCT) &&
        (struct_desc_compare(theStruct, accessedFromScope->parentFunction->implementedFor->data.asStruct) == 0))
    {
        return;
    }

    struct ScopeMember *accessed = scope_lookup(theStruct->members, name, E_VARIABLE);

    switch (accessed->accessibility)
    {
    // nothing to check if public
    case A_PUBLIC:
        break;

    case A_PRIVATE:
    {
        struct Scope *checkedScope = accessedFromScope;
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
        struct_check_field_access(theStruct, nameTree, scope, "Field");
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
    Iterator *fieldIter = NULL;
    for (fieldIter = deque_front(theStruct->fieldLocations); iterator_gettable(fieldIter); iterator_next(fieldIter))
    {
        struct StructField *field = iterator_get(fieldIter);
        try_resolve_generic_for_type(&field->variable->type, paramsMap, name, params);
    }
    iterator_free(fieldIter);
}
