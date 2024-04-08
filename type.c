#include "type.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

int Type_Compare(struct Type *typeA, struct Type *typeB)
{
    if (typeA->basicType != typeB->basicType)
    {
        return 1;
    }

    if (typeA->indirectionLevel != typeB->indirectionLevel)
    {
        return 2;
    }

    if (typeA->basicType == vt_class)
    {
        return strcmp(typeA->classType.name, typeB->classType.name);
    }

    return 0;
}

int Type_CompareAllowImplicitWidening(struct Type *typeA, struct Type *typeB)
{
    if (typeA->basicType != typeB->basicType)
    {
        switch (typeA->basicType)
        {
        case vt_null:
            return 1;

        case vt_any:
            switch (typeB->basicType)
            {
            case vt_null:
                return 1;
            case vt_class:
            case vt_any:
            case vt_u8:
            case vt_u16:
            case vt_u32:
                break;

            default:
                break;
            }
            break;

        case vt_u8:
            switch (typeB->basicType)
            {
            case vt_null:
            case vt_class:
                return 1;
            case vt_any:
            case vt_u8:
            case vt_u16:
            case vt_u32:
            case vt_u64:
                break;

            default:
                break;
            }
            break;

        case vt_u16:
            switch (typeB->basicType)
            {
            case vt_null:
            case vt_u8:
            case vt_class:
                return 1;
            case vt_any:
            case vt_u16:
            case vt_u32:
            case vt_u64:
                break;
            }
            break;

        case vt_u32:
            switch (typeB->basicType)
            {
            case vt_null:
            case vt_u8:
            case vt_u16:
            case vt_class:
                return 1;

            case vt_any:
            case vt_u32:
            case vt_u64:
                break;
            }
            break;

        case vt_u64:
            switch (typeB->basicType)
            {
            case vt_null:
            case vt_u8:
            case vt_u16:
            case vt_u32:
            case vt_class:
                return 1;

            case vt_any:
            case vt_u64:
                break;
            }
            break;

        case vt_class:
            switch (typeB->basicType)
            {
            case vt_null:
            case vt_u8:
            case vt_u16:
            case vt_u32:
            case vt_u64:
                return 1;

            case vt_any:
            case vt_class:
                break;
            }
        }
    }

    // allow implicit conversion from any type of pointer to 'any *' or 'any **', etc
    if ((typeA->indirectionLevel > 0) && (typeB->indirectionLevel > 0) && (typeB->basicType == vt_any))
    {
        return 0;
    }
    else if (typeA->indirectionLevel != typeB->indirectionLevel)
    {

        // both are arrays or both are not arrays
        if ((typeA->arraySize > 0) == (typeB->arraySize > 0))
        {
            return 2;
        }
        // only a is an array
        else if (typeA->arraySize > 0)
        {
            // b's indirection level should be a's + 1
            if (typeB->indirectionLevel != (typeA->indirectionLevel + 1))
            {
                return 2;
            }
        }
        else
        {
            // a's indirection level should be b's + 1
            if ((typeB->indirectionLevel + 1) != typeA->indirectionLevel)
            {
                return 2;
            }
        }
    }

    if (typeA->basicType == vt_class)
    {
        return strcmp(typeA->classType.name, typeB->classType.name);
    }
    return 0;
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
        len = sprintf(typeName, "%s", type->classType.name);
        break;

    default:
        ErrorAndExit(ERROR_INTERNAL, "Unexpected enum basicTypes value %d seen in Type_GetName!\n", type->basicType);
    }

    int indirectionCounter = 0;
    for (indirectionCounter = 0; indirectionCounter < type->indirectionLevel; indirectionCounter++)
    {
        typeName[len + indirectionCounter] = '*';
        len += sprintf(typeName + len, "*");
    }
    typeName[len + indirectionCounter] = '\0';

    if (type->arraySize > 0)
    {
        sprintf(typeName + len, "[%lu]", type->arraySize);
    }

    return typeName;
}
