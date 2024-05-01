#include "symtab_class.h"

#include "log.h"
#include "symtab_scope.h"
#include "util.h"

struct ClassEntry *createClass(struct Scope *scope,
                               char *name)
{
    struct ClassEntry *wipClass = malloc(sizeof(struct ClassEntry));
    wipClass->name = name;
    wipClass->members = Scope_new(scope, "CLASS", NULL);
    wipClass->memberLocations = Stack_New();
    wipClass->totalSize = 0;

    Scope_insert(scope, name, wipClass, e_class);
    return wipClass;
}

void assignOffsetToMemberVariable(struct ClassEntry *class,
                                  struct VariableEntry *variable)
{

    struct ClassMemberOffset *newMemberLocation = malloc(sizeof(struct ClassMemberOffset));

    // add the padding to the total size of the class
    class->totalSize += Scope_ComputePaddingForAlignment(class->members, &variable->type, class->totalSize);

    // place the new member at the (now aligned) current max size of the class
    if (class->totalSize > I64_MAX)
    {
        // TODO: implementation dependent size of size_t
        InternalError("Class %s has size too large (%zd bytes)!", class->name, class->totalSize);
    }
    newMemberLocation->offset = (ssize_t) class->totalSize;
    newMemberLocation->variable = variable;

    // add the size of the member we just added to the total size of the class
    class->totalSize += Type_GetSize(&variable->type, class->members);

    Stack_Push(class->memberLocations, newMemberLocation);
}

struct ClassMemberOffset *lookupMemberVariable(struct ClassEntry *class,
                                               struct AST *name)
{
    if (name->type != t_identifier)
    {
        LogTree(LOG_FATAL,
                name,
                "Non-identifier tree %s (%s) passed to Class_lookupOffsetOfMemberVariable!\n",
                name->value,
                getTokenName(name->type));
    }

    for (size_t memberIndex = 0; memberIndex < class->memberLocations->size; memberIndex++)
    {
        struct ClassMemberOffset *member = class->memberLocations->data[memberIndex];
        if (!strcmp(member->variable->name, name->value))
        {
            return member;
        }
    }

    LogTree(LOG_FATAL, name, "Use of nonexistent member variable %s in class %s", name->value, class->name);
    return NULL;
}

struct FunctionEntry *lookupMethod(struct ClassEntry *class,
                                   struct AST *name)
{
    for (size_t entryIndex = 0; entryIndex < class->members->entries->size; entryIndex++)
    {
        struct ScopeMember *examinedEntry = class->members->entries->data[entryIndex];
        if (!strcmp(examinedEntry->name, name->value))
        {
            if (examinedEntry->type != e_function)
            {
                LogTree(LOG_FATAL, name, "Attempt to call non-method member %s.%s as method!\n", class->name, name->value);
            }
            return examinedEntry->entry;
        }
    }

    LogTree(LOG_FATAL, name, "Attempt to call nonexistent method %s.%s\n", class->name, name->value);
    exit(1);
}

struct FunctionEntry *lookupMethodByString(struct ClassEntry *class,
                                           char *name)
{
    for (size_t entryIndex = 0; entryIndex < class->members->entries->size; entryIndex++)
    {
        struct ScopeMember *examinedEntry = class->members->entries->data[entryIndex];
        if (!strcmp(examinedEntry->name, name))
        {
            if (examinedEntry->type != e_function)
            {
                Log(LOG_FATAL, "Attempt to call non-method member %s.%s as method!\n", class->name, name);
            }
            return examinedEntry->entry;
        }
    }

    Log(LOG_FATAL, "Attempt to call nonexistent method %s.%s\n", class->name, name);
    exit(1);
}

struct ClassEntry *lookupClass(struct Scope *scope,
                               struct AST *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        LogTree(LOG_FATAL, name, "Use of undeclared class '%s'", name->value);
    }
    switch (lookedUp->type)
    {
    case e_class:
        return lookedUp->entry;

    default:
        LogTree(LOG_FATAL, name, "%s is not a class!", name->value);
    }

    return NULL;
}

struct ClassEntry *lookupClassByType(struct Scope *scope,
                                     struct Type *type)
{
    if (type->basicType != vt_class || type->nonArray.complexType.name == NULL)
    {
        InternalError("Non-class type or class type with null name passed to lookupClassByType!");
    }

    struct ScopeMember *lookedUp = Scope_lookup(scope, type->nonArray.complexType.name);
    if (lookedUp == NULL)
    {
        Log(LOG_FATAL, "Use of undeclared class '%s'", type->nonArray.complexType.name);
    }

    switch (lookedUp->type)
    {
    case e_class:
        return lookedUp->entry;

    default:
        InternalError("lookupClassByType for %s lookup got a non-class ScopeMember!", type->nonArray.complexType.name);
    }
}
