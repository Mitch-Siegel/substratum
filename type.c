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
                retVal = cantWiden;
                break;
            case vt_any:
            case vt_class:
                break;
            }
        }
    }
    return retVal;
}

int Type_CompareAllowImplicitWidening(struct Type *typeA, struct Type *typeB)
{
    const int indirectionMismatch = 2;

    int retVal = Type_CompareBasicTypeAllowImplicitWidening(typeA->basicType, typeB->basicType);
    if (retVal)
    {
        return retVal;
    }

    // allow implicit conversion from any type of pointer to 'any *' or 'any **', etc
    if ((typeA->indirectionLevel > 0) && (typeB->indirectionLevel > 0) && (typeB->basicType == vt_any))
    {
        retVal = 0;
    }
    else if (typeA->indirectionLevel != typeB->indirectionLevel)
    {

        // both are arrays or both are not arrays
        if ((typeA->arraySize > 0) == (typeB->arraySize > 0))
        {
            retVal = indirectionMismatch;
        }
        // only a is an array
        else if (typeA->arraySize > 0)
        {
            // b's indirection level should be a's + 1
            if (typeB->indirectionLevel != (typeA->indirectionLevel + 1))
            {
                retVal = indirectionMismatch;
            }
        }
        else
        {
            // a's indirection level should be b's + 1
            if ((typeB->indirectionLevel + 1) != typeA->indirectionLevel)
            {
                retVal = indirectionMismatch;
            }
        }
    }

    if (retVal)
    {
        return retVal;
    }

    if (typeA->basicType == vt_class)
    {
        retVal = strcmp(typeA->classType.name, typeB->classType.name);
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
