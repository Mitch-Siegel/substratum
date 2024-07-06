#include "symtab_enum.h"

#include "log.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include "util.h"

ssize_t enum_member_compare(void *enumMemberA, void *enumMemberB)
{
    return strcmp(((struct EnumMember *)enumMemberA)->name, ((struct EnumMember *)enumMemberB)->name);
}

void enum_entry_free(struct EnumEntry *the_enum)
{
    set_free(the_enum->members);

    free(the_enum);
}

struct EnumMember *enum_add_member(struct EnumEntry *theEnum,
                                   struct Ast *memberName,
                                   struct Type *memberType)
{
    struct EnumMember *newMember = malloc(sizeof(struct EnumMember));

    struct EnumEntry *existingEnumWithMember = scope_lookup_enum_by_member_name(theEnum->parentScope, memberName->value);
    if (existingEnumWithMember != NULL)
    {
        log_tree(LOG_FATAL, memberName, "Enum %s already has a member named %s", existingEnumWithMember->name, memberName->value);
    }

    newMember->name = memberName->value;
    newMember->numerical = theEnum->members->elements->size;
    
    newMember->type = *memberType;
    if(memberType->basicType != VT_NULL)
    {
        size_t memberSize = type_get_size(memberType, theEnum->parentScope);
        if(memberSize > theEnum->unionSize)
        {
            theEnum->unionSize = memberSize;
        }
    }

    set_insert(theEnum->members, newMember);

    return newMember;
}

struct EnumMember *enum_lookup_member(struct EnumEntry *theEnum,
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
