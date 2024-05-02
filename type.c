#include "type.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtab.h"

#include "symtab.h"

#include "log.h"
#include "util.h"

void Type_Init(struct Type *type)
{
    memset(type, 0, sizeof(struct Type));
}

void Type_Free(struct Type *type)
{
    free(type);
}

void Type_SetBasicType(struct Type *type, enum basicTypes basicType, char *complexTypeName, size_t pointerLevel)
{
    if (basicType == vt_class)
    {
        if (complexTypeName == NULL)
        {
            InternalError("Type_SetBasicType called with a null complexTypeName for vt_class!\n");
        }
    }
    else
    {
        if (complexTypeName != NULL)
        {
            InternalError("Type_SetBasicType called with a non-null complexTypeName for a non-vt_class type!\n");
        }
    }

    type->basicType = basicType;
    type->pointerLevel = pointerLevel;

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
        *type = liftedOutOfArray;
        type->pointerLevel += oldPointerLevel;
    }
}

ssize_t Type_Compare(struct Type *typeA, struct Type *typeB)
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

size_t Type_Hash(struct Type *type)
{
    size_t hash = 0;
    for (size_t byteIndex = 0; byteIndex < sizeof(struct Type); byteIndex++)
    {
        hash += ((u8 *)type)[byteIndex];
        hash <<= 1;
    }
    return hash;
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

int Type_CompareAllowImplicitWidening(struct Type *source, struct Type *dest)
{
    struct Type decayedSourceType = *source;
    Type_DecayArrays(&decayedSourceType);
    int retVal = Type_CompareBasicTypeAllowImplicitWidening(decayedSourceType.basicType, dest->basicType);
    if (retVal)
    {
        return retVal;
    }

    // allow implicit conversion from any type of pointer to 'any *' or 'any **', etc
    if ((source->pointerLevel > 0) && (dest->pointerLevel > 0) && (dest->basicType == vt_any))
    {
        retVal = 0;
    }
    else if (source->pointerLevel != dest->pointerLevel)
    {
        if (!((source->basicType == vt_array) && (dest->basicType == source->array.type->basicType) && (dest->pointerLevel == (source->array.type->pointerLevel + 1))))
        {
            if (source->pointerLevel > dest->pointerLevel)
            {
                retVal = 1;
            }
            else if (source->pointerLevel < dest->pointerLevel)
            {
                retVal = -1;
            }
        }
    }

    if (retVal)
    {
        return retVal;
    }

    // if we are converting from an array to something
    if (source->basicType == vt_array)
    {
        // if we haven't already returned by the time we get to here, we know that we are doing an implicit conversion such as 'something[123]->something*'
        // yank the arrayed type out, and decay its pointer manually, then recurse
        struct Type decayedType = *source->array.type;
        decayedType.pointerLevel++;
        retVal = Type_CompareAllowImplicitWidening(&decayedType, dest);
    }
    else if (source->basicType == vt_class)
    {
        retVal = strcmp(source->nonArray.complexType.name, dest->nonArray.complexType.name);
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
        InternalError("Unexpected enum basicTypes value %d seen in Type_GetName!", type->basicType);
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

struct Type *Type_Duplicate(struct Type *type)
{
    struct Type *dup = malloc(sizeof(struct Type));
    memcpy(dup, type, sizeof(struct Type));
    return dup;
}

size_t Type_GetSize(struct Type *type, struct Scope *scope)
{
    size_t size = 0;

    if (type->pointerLevel > 0)
    {
        size = MACHINE_REGISTER_SIZE_BYTES;
        return size;
    }

    switch (type->basicType)
    {
    case vt_null:
        InternalError("Type_GetSize called with basic type of vt_null!\n");
        break;

    case vt_any:
        // triple check that 'any' is only ever used as a pointer type a la c's void *
        if (type->pointerLevel == 0)
        {
            char *illegalAnyTypeName = Type_GetName(type);
            InternalError("Illegal 'any' type detected - %s\nSomething slipped through earlier sanity checks on use of 'any' as 'any *' or some other pointer type\n", illegalAnyTypeName);
        }
        size = sizeof(u8);
        break;

    case vt_u8:
        size = sizeof(u8);
        break;

    case vt_u16:
        size = sizeof(u16);
        break;

    case vt_u32:
        size = sizeof(u32);
        break;

    case vt_u64:
        size = sizeof(u64);
        break;

    case vt_class:
    {
        struct ClassEntry *class = lookupClassByType(scope, type);
        size = class->totalSize;
    }
    break;

    case vt_array:
    {
        struct Type typeRunner = *type;
        size = 1;
        while (typeRunner.basicType == vt_array)
        {
            size *= typeRunner.array.size;
            typeRunner = *typeRunner.array.type;
        }
        size *= Type_GetSize(&typeRunner, scope);
    }
    break;
    }

    return size;
}

size_t Type_GetSizeWhenDereferenced(struct Type *type, struct Scope *scope)
{
    if (type->pointerLevel == 0)
    {
        InternalError("Type_GetSizeWhenDereferenced called with non-pointer type %s!\n", Type_GetName(type));
    }
    struct Type dereferenced = *type;
    dereferenced.pointerLevel--;
    return Type_GetSize(&dereferenced, scope);
}

size_t Type_GetSizeOfArrayElement(struct Type *arrayType, struct Scope *scope)
{
    if (arrayType->basicType == vt_array)
    {
        struct Type element = *arrayType->array.type;
        return Type_GetSize(&element, scope);
    }
    if (arrayType->pointerLevel > 0)
    {
        struct Type element = *arrayType;
        element.pointerLevel--;
        return Type_GetSize(&element, scope);
    }

    InternalError("Type_GetSizeOfArrayElement called with non-array and non-pointer type %s!\n", Type_GetName(arrayType));
}

u8 Type_GetAlignment(struct Type *type, struct Scope *scope)
{
    u8 alignment = 0;
    switch (type->basicType)
    {
    case vt_class:
    {
        struct ClassEntry *class = lookupClassByType(scope, type);
        for (size_t memberIndex = 0; memberIndex < class->memberLocations->size; memberIndex++)
        {
            struct ClassMemberOffset *offset = class->memberLocations->data[memberIndex];
            u8 memberAlignment = Type_GetAlignment(&offset->variable->type, scope);
            if (memberAlignment > alignment)
            {
                alignment = memberAlignment;
            }
        }
    }
    break;

    case vt_array:
        alignment = alignSize(Type_GetSize(type->array.type, scope));
        break;

    default:
        alignment = alignSize(Type_GetSize(type, scope));
        break;
    }
    
    return alignment;
}

size_t Scope_ComputePaddingForAlignment(struct Scope *scope, struct Type *alignedType, size_t currentOffset)
{
    // calculate the number of bytes to which this member needs to be aligned
    size_t alignBytesForType = unalignSize(Type_GetAlignment(alignedType, scope));

    // compute how many bytes of padding we will need before this member to align it correctly
    size_t paddingRequired = 0;
    size_t bytesAfterAlignBoundary = currentOffset % alignBytesForType;
    if (bytesAfterAlignBoundary)
    {
        paddingRequired = alignBytesForType - bytesAfterAlignBoundary;
    }

    // add the padding to the total size of the class
    return paddingRequired;
}
