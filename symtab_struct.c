#include "symtab_struct.h"

#include "log.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include "util.h"

void struct_entry_free(struct StructEntry *theStruct)
{
    scope_free(theStruct->members);
    stack_free(theStruct->fieldLocations);
    free(theStruct);
}

void struct_assign_offset_to_field(struct StructEntry *memberOf,
                                   struct VariableEntry *variable)
{

    struct StructField *newMemberLocation = malloc(sizeof(struct StructField));

    // add the padding to the total size of the struct
    memberOf->totalSize += scope_compute_padding_for_alignment(memberOf->members, &variable->type, memberOf->totalSize);

    // place the new member at the (now aligned) current max size of the struct
    if (memberOf->totalSize > I64_MAX)
    {
        // TODO: implementation dependent size of size_t
        InternalError("Struct %s has size too large (%zd bytes)!", memberOf->name, memberOf->totalSize);
    }
    newMemberLocation->offset = (ssize_t)memberOf->totalSize;
    newMemberLocation->variable = variable;

    // add the size of the member we just added to the total size of the struct
    memberOf->totalSize += type_get_size(&variable->type, memberOf->members);
    log(LOG_DEBUG, "Assign offset %zu to member variable %s of struct %s - total struct size is now %zu", newMemberLocation->offset, variable->name, memberOf->name, memberOf->totalSize);

    stack_push(memberOf->fieldLocations, newMemberLocation);
}

// assuming we know that struct has a member with name identical to name->value, make sure we can actually access it
void struct_check_access(struct StructEntry *theStruct,
                         struct Ast *nameTree,
                         struct Scope *scope,
                         char *whatAccessingCalled)
{
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
void struct_check_access_by_name(struct StructEntry *theStruct,
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
            log(LOG_FATAL, "%s %s of struct %s has access specifier private - not accessible from this scope!", whatAccessingCalled, name, theStruct->name);
        }
        break;
    }
}

struct StructField *struct_lookup_field(struct StructEntry *theStruct,
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
    for (fieldIterator = stack_bottom(theStruct->fieldLocations); iterator_valid(fieldIterator); iterator_next(fieldIterator))
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
        log_tree(LOG_FATAL, nameTree, "Use of nonexistent member variable %s in struct %s", nameTree->value, theStruct->name);
    }
    else
    {
        struct_check_access(theStruct, nameTree, scope, "Member");
    }

    return returnedField;
}

struct StructField *struct_lookup_field_by_name(struct StructEntry *theStruct,
                                                char *name,
                                                struct Scope *scope)
{
    struct StructField *returnedField = NULL;
    Iterator *fieldIterator = NULL;
    for (fieldIterator = stack_bottom(theStruct->fieldLocations); iterator_valid(fieldIterator); iterator_next(fieldIterator))
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
        log(LOG_FATAL, "Use of nonexistent member variable %s in struct %s", name, theStruct->name);
    }
    else
    {
        struct_check_access_by_name(theStruct, name, scope, "Member");
    }

    return returnedField;
}

struct FunctionEntry *struct_looup_method(struct StructEntry *theStruct,
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

struct FunctionEntry *struct_lookup_associated_function(struct StructEntry *theStruct,
                                                        struct Ast *name,
                                                        struct Scope *scope)
{
    struct FunctionEntry *returendAssociated = NULL;

    struct ScopeMember *lookedUpEntry = scope_lookup_no_parent(theStruct->members, name->value, E_FUNCTION);

    if (lookedUpEntry == NULL)
    {
        log_tree(LOG_FATAL, name, "Attempt to call nonexistent associated function %s::%s\n", theStruct->name, name->value);
    }

    if (lookedUpEntry->type != E_FUNCTION)
    {
        log_tree(LOG_FATAL, name, "Attempt to call non-function member %s.%s as an associated function!\n", theStruct->name, name->value);
    }

    returendAssociated = lookedUpEntry->entry;

    struct_check_access(theStruct, name, scope, "Associated function");

    if (returendAssociated->isMethod)
    {
        log_tree(LOG_FATAL, name, "Attempt to call method %s.%s as an associated function!\n", theStruct->name, name->value);
    }

    return returendAssociated;
}

struct FunctionEntry *struct_lookup_method_by_string(struct StructEntry *theStruct,
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
