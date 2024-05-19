#include "symtab_struct.h"

#include "log.h"
#include "symtab_scope.h"
#include "util.h"

struct StructEntry *createStruct(struct Scope *scope,
                                 char *name)
{
    struct StructEntry *wipStruct = malloc(sizeof(struct StructEntry));
    wipStruct->name = name;
    wipStruct->members = Scope_new(scope, name, NULL);
    wipStruct->memberLocations = Stack_New();
    wipStruct->totalSize = 0;

    Scope_insert(scope, name, wipStruct, e_struct, a_public);
    return wipStruct;
}

void StructEntry_free(struct StructEntry *theStruct)
{
    Scope_free(theStruct->members);

    while (theStruct->memberLocations->size > 0)
    {
        free(Stack_Pop(theStruct->memberLocations));
    }

    Stack_Free(theStruct->memberLocations);
    free(theStruct);
}

void assignOffsetToMemberVariable(struct StructEntry *memberOf,
                                  struct VariableEntry *variable)
{

    struct StructMemberOffset *newMemberLocation = malloc(sizeof(struct StructMemberOffset));

    // add the padding to the total size of the struct
    memberOf->totalSize += Scope_ComputePaddingForAlignment(memberOf->members, &variable->type, memberOf->totalSize);

    // place the new member at the (now aligned) current max size of the struct
    if (memberOf->totalSize > I64_MAX)
    {
        // TODO: implementation dependent size of size_t
        InternalError("Struct %s has size too large (%zd bytes)!", memberOf->name, memberOf->totalSize);
    }
    newMemberLocation->offset = (ssize_t)memberOf->totalSize;
    newMemberLocation->variable = variable;

    // add the size of the member we just added to the total size of the struct
    memberOf->totalSize += Type_GetSize(&variable->type, memberOf->members);
    Log(LOG_DEBUG, "Assign offset %zu to member variable %s of struct %s - total struct size is now %zu", newMemberLocation->offset, variable->name, memberOf->name, memberOf->totalSize);

    Stack_Push(memberOf->memberLocations, newMemberLocation);
}

// assuming we know that struct has a member with name identical to name->value, make sure we can actually access it
void checkAccess(struct StructEntry *theStruct,
                 struct AST *name,
                 struct Scope *scope,
                 char *whatAccessingCalled)
{
    struct ScopeMember *accessed = Scope_lookup(theStruct->members, name->value);

    switch (accessed->accessibility)
    {
    // nothing to check if public
    case a_public:
        break;

    case a_private:
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
            LogTree(LOG_FATAL, name, "%s %s of struct %s has access specifier private - not accessible from this scope!", whatAccessingCalled, name->value, theStruct->name);
        }
        break;
    }
}

struct StructMemberOffset *lookupMemberVariable(struct StructEntry *theStruct,
                                                struct AST *name,
                                                struct Scope *scope)
{
    if (name->type != t_identifier)
    {
        LogTree(LOG_FATAL,
                name,
                "Non-identifier tree %s (%s) passed to Struct_lookupOffsetOfMemberVariable!\n",
                name->value,
                getTokenName(name->type));
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
        LogTree(LOG_FATAL, name, "Use of nonexistent member variable %s in struct %s", name->value, theStruct->name);
    }
    else
    {
        checkAccess(theStruct, name, scope, "Member");
    }

    return returnedMember;
}

struct FunctionEntry *lookupMethod(struct StructEntry *theStruct,
                                   struct AST *name,
                                   struct Scope *scope)
{
    struct FunctionEntry *returnedMethod = NULL;

    for (size_t entryIndex = 0; entryIndex < theStruct->members->entries->size; entryIndex++)
    {
        struct ScopeMember *examinedEntry = theStruct->members->entries->data[entryIndex];
        if (!strcmp(examinedEntry->name, name->value))
        {
            if (examinedEntry->type != e_function)
            {
                LogTree(LOG_FATAL, name, "Attempt to call non-method member %s.%s as method!\n", theStruct->name, name->value);
            }
            returnedMethod = examinedEntry->entry;
        }
    }

    if (returnedMethod == NULL)
    {
        LogTree(LOG_FATAL, name, "Attempt to call nonexistent method %s.%s\n", theStruct->name, name->value);
    }
    else
    {
        checkAccess(theStruct, name, scope, "Method");
    }

    return returnedMethod;
}

struct FunctionEntry *lookupMethodByString(struct StructEntry *theStruct,
                                           char *name)
{
    for (size_t entryIndex = 0; entryIndex < theStruct->members->entries->size; entryIndex++)
    {
        struct ScopeMember *examinedEntry = theStruct->members->entries->data[entryIndex];
        if (!strcmp(examinedEntry->name, name))
        {
            if (examinedEntry->type != e_function)
            {
                Log(LOG_FATAL, "Attempt to call non-method member %s.%s as method!\n", theStruct->name, name);
            }
            return examinedEntry->entry;
        }
    }

    Log(LOG_FATAL, "Attempt to call nonexistent method %s.%s\n", theStruct->name, name);
    exit(1);
}

struct StructEntry *lookupStruct(struct Scope *scope,
                                 struct AST *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        LogTree(LOG_FATAL, name, "Use of undeclared struct '%s'", name->value);
    }
    switch (lookedUp->type)
    {
    case e_struct:
        return lookedUp->entry;

    default:
        LogTree(LOG_FATAL, name, "%s is not a struct!", name->value);
    }

    return NULL;
}

struct StructEntry *lookupStructByType(struct Scope *scope,
                                       struct Type *type)
{
    if (type->basicType != vt_struct || type->nonArray.complexType.name == NULL)
    {
        InternalError("Non-struct type or struct type with null name passed to lookupStructByType!");
    }

    struct ScopeMember *lookedUp = Scope_lookup(scope, type->nonArray.complexType.name);
    if (lookedUp == NULL)
    {
        Log(LOG_FATAL, "Use of undeclared struct '%s'", type->nonArray.complexType.name);
    }

    switch (lookedUp->type)
    {
    case e_struct:
        return lookedUp->entry;

    default:
        InternalError("lookupStructByType for %s lookup got a non-struct ScopeMember!", type->nonArray.complexType.name);
    }
}
