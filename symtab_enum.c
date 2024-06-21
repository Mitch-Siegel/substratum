#include "symtab_enum.h"

#include "log.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include "util.h"

ssize_t EnumMember_Compare(void *enumMemberA, void *enumMemberB)
{
    return strcmp(((struct EnumMember *)enumMemberA)->name, ((struct EnumMember *)enumMemberB)->name);
}

struct EnumEntry *createEnum(struct Scope *scope,
                             char *name)
{
    struct EnumEntry *wipEnum = malloc(sizeof(struct EnumEntry));
    wipEnum->name = name;
    wipEnum->members = Set_New(EnumMember_Compare, free);

    Scope_insert(scope, name, wipEnum, e_enum, a_public);
    return wipEnum;
}

void EnumEntry_free(struct EnumEntry *theEnum)
{
    Set_Free(theEnum->members);

    free(theEnum);
}

struct EnumMember *addEnumMember(struct EnumEntry *theEnum,
                                 struct AST *name)
{
    struct EnumMember *newMember = malloc(sizeof(struct EnumMember));

    newMember->name = name->value;
    newMember->numerical = theEnum->members->elements->size;
    Set_Insert(theEnum->members, newMember);

    return newMember;
}

struct EnumEntry *lookupEnum(struct Scope *scope,
                             struct AST *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        LogTree(LOG_FATAL, name, "Use of undeclared enum '%s'", name->value);
    }
    switch (lookedUp->type)
    {
    case e_enum:
        return lookedUp->entry;

    default:
        LogTree(LOG_FATAL, name, "%s is not an enum!", name->value);
    }

    return NULL;
}

struct EnumEntry *lookupEnumByType(struct Scope *scope,
                                   struct Type *type)
{
    if (type->basicType != vt_enum || type->nonArray.complexType.name == NULL)
    {
        InternalError("Non-enum type or enum type with null name passed to lookupEnumByType!");
    }

    struct ScopeMember *lookedUp = Scope_lookup(scope, type->nonArray.complexType.name);
    if (lookedUp == NULL)
    {
        Log(LOG_FATAL, "Use of undeclared enum '%s'", type->nonArray.complexType.name);
    }

    switch (lookedUp->type)
    {
    case e_enum:
        return lookedUp->entry;

    default:
        InternalError("lookupEnumByType for %s lookup got a non-struct ScopeMember!", type->nonArray.complexType.name);
    }
}