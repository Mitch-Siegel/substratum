#include "enum_desc.h"

#include "log.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include "util.h"

ssize_t enum_member_compare(void *enumMemberA, void *enumMemberB)
{
    return strcmp(((struct EnumMember *)enumMemberA)->name, ((struct EnumMember *)enumMemberB)->name);
}

struct EnumDesc *enum_desc_new(char *name, struct Scope *parentScope)
{
    struct EnumDesc *wipEnum = malloc(sizeof(struct EnumDesc));
    wipEnum->name = name;
    wipEnum->parentScope = parentScope;
    wipEnum->members = set_new(free, (ssize_t(*)(void *, void *))enum_member_compare);
    wipEnum->unionSize = 0;

    return wipEnum;
}

void enum_desc_free(struct EnumDesc *theEnum)
{
    set_free(theEnum->members);
    free(theEnum);
}

struct EnumMember *enum_add_member_by_name(struct EnumDesc *theEnum,
                                           char *memberName,
                                           struct Type *memberType)
{
    struct EnumMember *newMember = malloc(sizeof(struct EnumMember));
    newMember->name = memberName;
    newMember->numerical = theEnum->members->size;
    newMember->type = *memberType;

    set_insert(theEnum->members, newMember);

    return newMember;
}

struct EnumMember *enum_add_member(struct EnumDesc *theEnum,
                                   struct Ast *memberName,
                                   struct Type *memberType)
{

    // TODO: handle generics
    struct EnumDesc *existingEnumWithMember = scope_lookup_enum_by_member_name(theEnum->parentScope, memberName->value);
    if (existingEnumWithMember != NULL)
    {
        log_tree(LOG_FATAL, memberName, "Enum %s already has a member named %s", existingEnumWithMember->name, memberName->value);
    }

    return enum_add_member_by_name(theEnum, memberName->value, memberType);
}

struct EnumMember *enum_lookup_member(struct EnumDesc *theEnum,
                                      struct Ast *name)
{
    struct EnumMember dummyMember = {0};
    dummyMember.name = name->value;
    struct EnumMember *lookedUp = set_find(theEnum->members, &dummyMember);

    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, name, "Use of undeclared member %s in enum %s", name->value, theEnum->name);
    }

    return lookedUp;
}

struct EnumDesc *enum_desc_clone(struct EnumDesc *toClone, char *name)
{
    struct EnumDesc *newEnum = enum_desc_new(name, toClone->parentScope);

    Iterator *memberIter = NULL;
    for (memberIter = set_begin(toClone->members); iterator_gettable(memberIter); iterator_next(memberIter))
    {
        struct EnumMember *member = iterator_get(memberIter);
        struct EnumMember *newMember = malloc(sizeof(struct EnumMember));
        newMember->name = member->name;
        newMember->numerical = member->numerical;
        newMember->type = member->type;

        set_insert(newEnum->members, newMember);
    }
    iterator_free(memberIter);

    return newEnum;
}

void enum_desc_calculate_union_size(struct EnumDesc *theEnum)
{
    size_t maxUnionSize = 0;
    Iterator *memberIter = NULL;
    for (memberIter = set_begin(theEnum->members); iterator_gettable(memberIter); iterator_next(memberIter))
    {
        struct EnumMember *member = iterator_get(memberIter);

        if (member->type.basicType != VT_NULL)
        {
            size_t memberSize = type_get_size(&member->type, theEnum->parentScope);
            if (memberSize > maxUnionSize)
            {
                maxUnionSize = memberSize;
            }
        }
    }
    iterator_free(memberIter);

    theEnum->unionSize = maxUnionSize;
}

void enum_desc_print(struct EnumDesc *theEnum, size_t depth, FILE *outFile)
{
    Iterator *memberIter = NULL;
    for (memberIter = set_begin(theEnum->members); iterator_gettable(memberIter); iterator_next(memberIter))
    {
        struct EnumMember *member = iterator_get(memberIter);
        for (size_t i = 0; i < depth; i++)
        {
            fprintf(outFile, "  ");
        }
        char *typeString = type_get_name(&member->type);
        fprintf(outFile, "%s(%s) = %zu,\n", member->name, typeString, member->numerical);
        free(typeString);
    }
    iterator_free(memberIter);
}

void enum_desc_resolve_generics(struct EnumDesc *theEnum, HashTable *paramsMap, char *name, List *params)
{
    Iterator *memberIter = NULL;
    for (memberIter = set_begin(theEnum->members); iterator_gettable(memberIter); iterator_next(memberIter))
    {
        struct EnumMember *member = iterator_get(memberIter);
        char *oldTypeName = type_get_name(&member->type);
        type_try_resolve_generic(&member->type, paramsMap, name, params);
        char *newTypeName = type_get_name(&member->type);
        log(LOG_DEBUG, "Attempting to resolve member %s::%s: old type %s, new type %s", theEnum->name, member->name, oldTypeName, newTypeName);

        free(oldTypeName);
        free(newTypeName);
    }
    iterator_free(memberIter);

    enum_desc_calculate_union_size(theEnum);
}
