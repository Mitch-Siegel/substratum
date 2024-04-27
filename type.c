#include "type.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtab.h"

#include "util.h"

void Type_Init(struct Type *type)
{
    memset(type, 0, sizeof(struct Type));
}

struct Type *Type_New()
{
    struct Type *wip = malloc(sizeof(struct Type));
    Type_Init(wip);
    return wip;
}

void Type_SetBasicType(struct Type *type, enum basicTypes basicType, char *complexTypeName, size_t pointerLevel)
{
    if (type->basicType == vt_class)
    {
        if (complexTypeName == NULL)
        {
            ErrorAndExit(ERROR_INTERNAL, "Type_SetBasicType called with a null complexTypeName for vt_class!\n");
        }
    }
    else
    {
        if (complexTypeName != NULL)
        {
            ErrorAndExit(ERROR_INTERNAL, "Type_SetBasicType called with a non-null complexTypeName for a non-vt_class type!\n");
        }
    }

    type->basicType = basicType;
    type->pointerLevel = pointerLevel;
    ;
    if (basicType == vt_class)
    {
        type->nonArray.complexType.name = complexTypeName;
    }
}

size_t Type_GetIndirectionLevel(struct Type *type)
{
    size_t indirectionLevel = type->pointerLevel;
    if (type->basicType == vt_array)
    {
        indirectionLevel++;
        indirectionLevel += Type_GetIndirectionLevel(type->array.type);
    }

    return indirectionLevel;
}

void Type_DecayArrays(struct Type *type)
{
    while (type->basicType == vt_array)
    {
        size_t oldPointerLevel = type->pointerLevel + 1;
        struct Type liftedOutOfArray = *type->array.type;
        free(type->array.type);
        *type = liftedOutOfArray;
        type->pointerLevel += oldPointerLevel;
    }
}

int Type_Compare(struct Type *typeA, struct Type *typeB)
{
    if (typeA->basicType != typeB->basicType)
    {
        return 1;
    }

    if (typeA->basicType == vt_array)
    {
        if (typeA->array.size > typeB->array.size)
        {
            return 1;
        }

        if (typeB->array.size > typeA->array.size)
        {
            return -1;
        }

        // TODO: compare initializeArrayTo values?

        return Type_Compare(typeA->array.type, typeB->array.type);
    }

    return 0;
}

int Type_CompareBasicTypeAllowImplicitWidening(enum basicTypes basicTypeA, enum basicTypes basicTypeB)
{
    int retVal = 0;
    const int cantWiden = 1;
    if (basicTypeA != basicTypeB)
    {
        switch (basicTypeA)
        {
        case vt_null:
            retVal = cantWiden;
            break;

        case vt_any:
            switch (basicTypeB)
            {
            case vt_null:
            case vt_array:
                retVal = cantWiden;
                break;
            case vt_class:
            case vt_any:
            case vt_u8:
            case vt_u16:
            case vt_u32:
            case vt_u64:
                break;
            }
            break;

        case vt_u8:
            switch (basicTypeB)
            {
            case vt_null:
            case vt_class:
            case vt_array:
                retVal = cantWiden;
                break;
            case vt_any:
            case vt_u8:
            case vt_u16:
            case vt_u32:
            case vt_u64:
                break;
            }
            break;

        case vt_u16:
            switch (basicTypeB)
            {
            case vt_null:
            case vt_u8:
            case vt_class:
            case vt_array:
                retVal = cantWiden;
                break;
            case vt_any:
            case vt_u16:
            case vt_u32:
            case vt_u64:
                break;
            }
            break;

        case vt_u32:
            switch (basicTypeB)
            {
            case vt_null:
            case vt_u8:
            case vt_u16:
            case vt_class:
            case vt_array:
                retVal = cantWiden;
                break;

            case vt_any:
            case vt_u32:
            case vt_u64:
                break;
            }
            break;

        case vt_u64:
            switch (basicTypeB)
            {
            case vt_null:
            case vt_u8:
            case vt_u16:
            case vt_u32:
            case vt_class:
            case vt_array:
                retVal = cantWiden;
                break;

            case vt_any:
            case vt_u64:
                break;
            }
            break;

        case vt_class:
            switch (basicTypeB)
            {
            case vt_null:
            case vt_u8:
            case vt_u16:
            case vt_u32:
            case vt_u64:
            case vt_array:
                retVal = cantWiden;
                break;
            case vt_any:
            case vt_class:
                break;
            }
            break;

        case vt_array:
        {
            switch (basicTypeB)
            {
            case vt_null:
            case vt_u8:
            case vt_u16:
            case vt_u32:
            case vt_u64:
            case vt_class:
                retVal = cantWiden;
                break;
            case vt_any:
            case vt_array:
                break;
            }
            break;
        }
        break;
        }
    }
    return retVal;
}

int Type_CompareAllowImplicitWidening(struct Type *typeA, struct Type *typeB)
{
    int retVal = Type_CompareBasicTypeAllowImplicitWidening(typeA->basicType, typeB->basicType);
    if (retVal)
    {
        return retVal;
    }
    // allow implicit conversion from any type of pointer to 'any *' or 'any **', etc
    if ((typeA->pointerLevel > 0) && (typeB->pointerLevel > 0) && (typeB->basicType == vt_any))
    {
        retVal = 0;
    }
    else if (typeA->pointerLevel != typeB->pointerLevel)
    {
        if (typeA->pointerLevel > typeB->pointerLevel)
        {
            retVal = 1;
        }
        else if (typeA->pointerLevel < typeB->pointerLevel)
        {
            retVal = -1;
        }
    }

    if (retVal)
    {
        return retVal;
    }
    if (typeA->basicType == vt_array)
    {
        retVal = Type_CompareAllowImplicitWidening(typeA->array.type, typeB->array.type);
    }
    else if (typeA->basicType == vt_class)
    {
        retVal = strcmp(typeA->nonArray.complexType.name, typeB->nonArray.complexType.name);
    }

    return retVal;
}

char *Type_GetName(struct Type *type)
{
    const u32 sprintTypeNameLength = 1024;
    char *typeName = malloc(sprintTypeNameLength * sizeof(char));
    int len = 0;
    switch (type->basicType)
    {
    case vt_null:
        len = sprintf(typeName, "NOTYPE");
        break;

    case vt_any:
        len = sprintf(typeName, "any");
        break;

    case vt_u8:
        len = sprintf(typeName, "u8");
        break;

    case vt_u16:
        len = sprintf(typeName, "u16");
        break;

    case vt_u32:
        len = sprintf(typeName, "u32");
        break;

    case vt_u64:
        len = sprintf(typeName, "u64");
        break;

    case vt_class:
        len = sprintf(typeName, "%s", type->nonArray.complexType.name);
        break;

    case vt_array:
    {
        char *arrayTypeName = Type_GetName(type->array.type);
        len = sprintf(typeName, "%s[%zu]", arrayTypeName, type->array.size);
        free(arrayTypeName);
    }
    break;

    default:
        ErrorAndExit(ERROR_INTERNAL, "Unexpected enum basicTypes value %d seen in Type_GetName!\n", type->basicType);
    }

    int pointerCounter = 0;
    for (pointerCounter = 0; pointerCounter < type->pointerLevel; pointerCounter++)
    {
        typeName[len + pointerCounter] = '*';
        len += sprintf(typeName + len, "*");
    }
    typeName[len + pointerCounter] = '\0';

    return typeName;
}

u8 Type_GetAlignment(struct Scope *scope, struct Type *type)
{
    struct Type actualType = *type;
    u8 alignBits = 0;
    while (type->basicType == vt_array)
    {
        actualType = *type->array.type;
    }

    switch (actualType.basicType)
    {
    // the compiler is becoming the compilee
    case vt_any:
        if (type->pointerLevel == 0)
        {
            ErrorAndExit(ERROR_INTERNAL, "Saw vt_any with pointerLevel 0 in Type_GetAlignment!\n");
        }
        break;

    case vt_u8:
        alignBits = alignSize(sizeof(u8));
        break;

    case vt_u16:
        alignBits = alignSize(sizeof(u16));
        break;

    case vt_u32:
        alignBits = alignSize(sizeof(u32));
        break;

    case vt_u64:
        alignBits = alignSize(sizeof(u64));
        break;

    case vt_class:
    {
        struct ClassEntry *class = lookupClassByType(scope, type);

        for (size_t memberIndex = 0; memberIndex < class->memberLocations->size; memberIndex++)
        {
            struct ClassMemberOffset *examinedMember = (struct ClassMemberOffset *)class->memberLocations->data[memberIndex];

            u8 examinedMemberAlignment = getAlignmentOfType(scope, &examinedMember->variable->type);
            if (examinedMemberAlignment > alignBits)
            {
                alignBits = examinedMemberAlignment;
            }
        }
    }
    break;

    case vt_array:
        ErrorAndExit(ERROR_INTERNAL, "Saw vt_array after scraping down array types in Type_GetAlignment!\n");

    case vt_null:
        ErrorAndExit(ERROR_INTERNAL, "Saw vt_null in Type_GetAlignment!\n");
    }

    // if this is a pointer, it needs to be aligned to the size of a pointer irrespective of the type it points to
    if (type->pointerLevel > 0)
    {
        alignBits = alignSize(sizeof(size_t));
    }

    return alignBits;
}
