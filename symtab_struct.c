#include "symtab_struct.h"

#include "log.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include "util.h"

struct StructEntry *create_struct(struct Scope *scope,
                                 char *name)
{
    struct StructEntry *wipStruct = malloc(sizeof(struct StructEntry));
    wipStruct->name = name;
    wipStruct->members = scope_new(scope, name, NULL, wipStruct);
    wipStruct->memberLocations = stack_new();
    wipStruct->totalSize = 0;

    scope_insert(scope, name, wipStruct, E_STRUCT, A_PUBLIC);
    return wipStruct;
}

void struct_entry_free(struct StructEntry *theStruct)
{
    scope_free(theStruct->members);

    while (theStruct->memberLocations->size > 0)
    {
        free(stack_pop(theStruct->memberLocations));
    }

    stack_free(theStruct->memberLocations);
    free(theStruct);
}

void assign_offset_to_member_variable(struct StructEntry *memberOf,
                                  struct VariableEntry *variable)
{

    struct StructMemberOffset *newMemberLocation = malloc(sizeof(struct StructMemberOffset));

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

    stack_push(memberOf->memberLocations, newMemberLocation);
}

// assuming we know that struct has a member with name identical to name->value, make sure we can actually access it
void check_access(struct StructEntry *theStruct,
                 struct AST *name,
                 struct Scope *scope,
                 char *whatAccessingCalled)
{
    struct ScopeMember *accessed = scope_lookup(theStruct->members, name->value);

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
            log_tree(LOG_FATAL, name, "%s %s of struct %s has access specifier private - not accessible from this scope!", whatAccessingCalled, name->value, theStruct->name);
        }
        break;
    }
}

struct StructMemberOffset *lookup_member_variable(struct StructEntry *theStruct,
                                                struct AST *name,
                                                struct Scope *scope)
{
    if (name->type != T_IDENTIFIER)
    {
        log_tree(LOG_FATAL,
                name,
                "Non-identifier tree %s (%s) passed to Struct_lookupOffsetOfMemberVariable!\n",
                name->value,
                get_token_name(name->type));
    }

    struct StructMemberOffset *returnedMember = NULL;
    for (size_t memberIndex = 0; memberIndex < theStruct->memberLocations->size; memberIndex++)
    {
        struct StructMemberOffset *member = theStruct->memberLocations->data[memberIndex];
        if (!strcmp(member->variable->name, name->value))
        {
            returnedMember = member;
            break;
        }
    }

    if (returnedMember == NULL)
    {
        log_tree(LOG_FATAL, name, "Use of nonexistent member variable %s in struct %s", name->value, theStruct->name);
    }
    else
    {
        check_access(theStruct, name, scope, "Member");
    }

    return returnedMember;
}

struct FunctionEntry *looup_method(struct StructEntry *theStruct,
                                  struct AST *name,
                                  struct Scope *scope)
{
    struct FunctionEntry *returnedMethod = NULL;

    for (size_t entryIndex = 0; entryIndex < theStruct->members->entries->size; entryIndex++)
    {
        struct ScopeMember *examinedEntry = theStruct->members->entries->data[entryIndex];
        if (!strcmp(examinedEntry->name, name->value))
        {
            if (examinedEntry->type != E_FUNCTION)
            {
                log_tree(LOG_FATAL, name, "Attempt to call non-method member %s.%s as method!\n", theStruct->name, name->value);
            }
            returnedMethod = examinedEntry->entry;
        }
    }

    if (returnedMethod == NULL)
    {
        log_tree(LOG_FATAL, name, "Attempt to call nonexistent method %s.%s\n", theStruct->name, name->value);
    }
    else
    {
        check_access(theStruct, name, scope, "Method");
    }

    return returnedMethod;
}

struct FunctionEntry *lookup_associated_function(struct StructEntry *theStruct,
                                               struct AST *name,
                                               struct Scope *scope)
{
    struct FunctionEntry *returnedAssociated = NULL;

    for (size_t entryIndex = 0; entryIndex < theStruct->members->entries->size; entryIndex++)
    {
        struct ScopeMember *examinedEntry = theStruct->members->entries->data[entryIndex];
        if (!strcmp(examinedEntry->name, name->value))
        {
            if (examinedEntry->type != E_FUNCTION)
            {
                log_tree(LOG_FATAL, name, "Attempt to call %s.%s as an associated function!\n", theStruct->name, name->value);
            }
            returnedAssociated = examinedEntry->entry;

            if (returnedAssociated->isMethod)
            {
                // TODO: function prototype printing
                log_tree(LOG_FATAL, name, "Attempt to call method %s.%s() as an associated function!\n", theStruct->name, name->value);
            }
        }
    }

    if (returnedAssociated == NULL)
    {
        log_tree(LOG_FATAL, name, "Attempt to call nonexistent associated function %s::%s\n", theStruct->name, name->value);
    }
    else
    {
        check_access(theStruct, name, scope, "Associated");
    }

    return returnedAssociated;
}

struct FunctionEntry *lookup_method_by_string(struct StructEntry *theStruct,
                                           char *name)
{
    for (size_t entryIndex = 0; entryIndex < theStruct->members->entries->size; entryIndex++)
    {
        struct ScopeMember *examinedEntry = theStruct->members->entries->data[entryIndex];
        if (!strcmp(examinedEntry->name, name))
        {
            if (examinedEntry->type != E_FUNCTION)
            {
                log(LOG_FATAL, "Attempt to call non-method member %s.%s as method!\n", theStruct->name, name);
            }
            return examinedEntry->entry;
        }
    }

    log(LOG_FATAL, "Attempt to call nonexistent method %s.%s\n", theStruct->name, name);
    exit(1);
}

struct StructEntry *lookup_struct(struct Scope *scope,
                                 struct AST *name)
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

struct StructEntry *lookup_struct_by_type(struct Scope *scope,
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
