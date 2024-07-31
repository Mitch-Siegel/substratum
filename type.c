#include "type.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtab.h"

#include "log.h"
#include "util.h"

void type_init(struct Type *type)
{
    memset(type, 0, sizeof(struct Type));
}

void type_free(struct Type *type)
{
    free(type);
}

void type_set_basic_type(struct Type *type, enum BASIC_TYPES basicType, char *complexTypeName, size_t pointerLevel)
{
    if ((basicType == VT_STRUCT) || (basicType == VT_ENUM))
    {
        if (complexTypeName == NULL)
        {
            InternalError("Type_SetBasicType called with a null complexTypeName for VT_STRUCT!\n");
        }
    }
    else
    {
        if (complexTypeName != NULL)
        {
            InternalError("Type_SetBasicType called with a non-null complexTypeName for a non-VT_STRUCT type!\n");
        }
    }

    type->basicType = basicType;
    type->pointerLevel = pointerLevel;

    if ((basicType == VT_STRUCT) || (basicType == VT_ENUM))
    {
        type->nonArray.complexType.name = complexTypeName;
    }
}

size_t type_get_indirection_level(struct Type *type)
{
    size_t indirectionLevel = type->pointerLevel;
    if (type->basicType == VT_ARRAY)
    {
        indirectionLevel++;
        indirectionLevel += type_get_indirection_level(type->array.type);
    }

    return indirectionLevel;
}

// decay at most one level of arrays
void type_single_decay(struct Type *type)
{
    if (type->basicType == VT_ARRAY)
    {
        size_t oldPointerLevel = type->pointerLevel + 1;
        struct Type liftedOutOfArray = *type->array.type;
        *type = liftedOutOfArray;
        type->pointerLevel += oldPointerLevel;
    }
}

void type_decay_arrays(struct Type *type)
{
    while (type->basicType == VT_ARRAY)
    {
        size_t oldPointerLevel = type->pointerLevel + 1;
        struct Type liftedOutOfArray = *type->array.type;
        *type = liftedOutOfArray;
        type->pointerLevel += oldPointerLevel;
    }
}

void type_copy_decay_arrays(struct Type *dest, struct Type *src)
{
    *dest = *src;
    type_decay_arrays(dest);
}

ssize_t type_compare(struct Type *typeA, struct Type *typeB)
{
    if (typeA->basicType != typeB->basicType)
    {
        return 1;
    }

    if (typeA->basicType == VT_ARRAY)
    {
        if (typeA->array.size != typeB->array.size)
        {
            return (ssize_t)typeA->array.size - (ssize_t)typeB->array.size;
        }

        // TODO: compare initializeArrayTo values?
        return type_compare(typeA->array.type, typeB->array.type);
    }

    return 0;
}

size_t type_hash(struct Type *type)
{
    size_t hash = 0;
    for (size_t byteIndex = 0; byteIndex < sizeof(struct Type); byteIndex++)
    {
        hash += ((u8 *)type)[byteIndex];
        hash <<= 1;
    }
    return hash;
}

bool type_is_object(struct Type *type)
{
    return type_is_array_object(type) ||
           type_is_struct_object(type) ||
           type_is_enum_object(type);
}

bool type_is_array_object(struct Type *type)
{
    return ((type->basicType == VT_ARRAY) && type->pointerLevel == 0);
}

bool type_is_struct_object(struct Type *type)
{
    return ((type->basicType == VT_STRUCT) && (type->pointerLevel == 0));
}

bool type_is_enum_object(struct Type *type)
{
    return ((type->basicType == VT_ENUM) && (type->pointerLevel == 0));
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
int type_compare_basic_type_allow_implicit_widening(enum BASIC_TYPES basicTypeA, enum BASIC_TYPES basicTypeB)
{
    int retVal = 0;
    const int CANT_WIDEN = 1;

    if (basicTypeA != basicTypeB)
    {
        switch (basicTypeA)
        {
        case VT_NULL:
            retVal = CANT_WIDEN;
            break;

        case VT_ANY:
            switch (basicTypeB)
            {
            case VT_NULL:
            case VT_ARRAY:
            case VT_GENERIC_PARAM:
                retVal = CANT_WIDEN;
                break;
            case VT_ENUM:
            case VT_STRUCT:
            case VT_ANY:
            case VT_U8:
            case VT_U16:
            case VT_U32:
            case VT_U64:
                break;
            }
            break;

        case VT_U8:
            switch (basicTypeB)
            {
            case VT_NULL:
            case VT_ENUM:
            case VT_STRUCT:
            case VT_ARRAY:
            case VT_GENERIC_PARAM:
                retVal = CANT_WIDEN;
                break;
            case VT_ANY:
            case VT_U8:
            case VT_U16:
            case VT_U32:
            case VT_U64:
                break;
            }
            break;

        case VT_U16:
            switch (basicTypeB)
            {
            case VT_NULL:
            case VT_U8:
            case VT_ENUM:
            case VT_STRUCT:
            case VT_ARRAY:
            case VT_GENERIC_PARAM:
                retVal = CANT_WIDEN;
                break;
            case VT_ANY:
            case VT_U16:
            case VT_U32:
            case VT_U64:
                break;
            }
            break;

        case VT_U32:
            switch (basicTypeB)
            {
            case VT_NULL:
            case VT_U8:
            case VT_U16:
            case VT_ENUM:
            case VT_STRUCT:
            case VT_ARRAY:
            case VT_GENERIC_PARAM:
                retVal = CANT_WIDEN;
                break;

            case VT_ANY:
            case VT_U32:
            case VT_U64:
                break;
            }
            break;

        case VT_U64:
            switch (basicTypeB)
            {
            case VT_NULL:
            case VT_U8:
            case VT_U16:
            case VT_U32:
            case VT_ENUM:
            case VT_STRUCT:
            case VT_ARRAY:
            case VT_GENERIC_PARAM:
                retVal = CANT_WIDEN;
                break;

            case VT_ANY:
            case VT_U64:
                break;
            }
            break;

        case VT_STRUCT:
            switch (basicTypeB)
            {
            case VT_NULL:
            case VT_U8:
            case VT_U16:
            case VT_U32:
            case VT_U64:
            case VT_ARRAY:
            case VT_GENERIC_PARAM:
            case VT_ENUM:
                retVal = CANT_WIDEN;
                break;
            case VT_ANY:
            case VT_STRUCT:
                break;
            }
            break;

        case VT_ENUM:
        {
            switch (basicTypeB)
            {
            case VT_NULL:
            case VT_U8:
            case VT_U16:
            case VT_U32:
            case VT_U64:
            case VT_STRUCT:
            case VT_ARRAY:
            case VT_GENERIC_PARAM:
                retVal = CANT_WIDEN;
                break;
            case VT_ANY:
            case VT_ENUM:
                break;
            }
            break;
        }
        break;

        case VT_ARRAY:
        {
            switch (basicTypeB)
            {
            case VT_NULL:
            case VT_U8:
            case VT_U16:
            case VT_U32:
            case VT_U64:
            case VT_GENERIC_PARAM:
            case VT_ENUM:
            case VT_STRUCT:
                retVal = CANT_WIDEN;
                break;
            case VT_ANY:
            case VT_ARRAY:
                break;
            }
            break;
        }
        break;

        case VT_GENERIC_PARAM:
        {
            retVal = CANT_WIDEN;
        }
        break;
        }
    }
    return retVal;
}
// NOLINTEND(readability-function-cognitive-complexity)

int type_compare_allow_implicit_widening(struct Type *src, struct Type *dest)
{
    struct Type decayedSourceType = *src;
    type_decay_arrays(&decayedSourceType);

    struct Type decayedDestType = *dest;
    type_decay_arrays(&decayedDestType);
    int retVal = type_compare_basic_type_allow_implicit_widening(decayedSourceType.basicType, decayedDestType.basicType);
    if (retVal)
    {
        return retVal;
    }

    // allow implicit conversion from any type of pointer to 'any *' or 'any **', etc
    if (((src->pointerLevel > 0) || (src->basicType == VT_ARRAY)) &&
        (dest->pointerLevel > 0) &&
        (dest->basicType == VT_ANY))
    {
        retVal = 0;
    }
    else if (src->pointerLevel != dest->pointerLevel)
    {
        if (!((src->basicType == VT_ARRAY) && (dest->basicType == src->array.type->basicType) && (dest->pointerLevel == (src->array.type->pointerLevel + 1))))
        {
            if (src->pointerLevel > dest->pointerLevel)
            {
                retVal = 1;
            }
            else if (src->pointerLevel < dest->pointerLevel)
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
    if (src->basicType == VT_ARRAY)
    {
        // if we haven't already returned by the time we get to here, we know that we are doing an implicit conversion such as 'something[123]->something*'
        // yank the arrayed type out, and decay its pointer manually, then recurse
        struct Type singleDecayedSourceType = *src;
        type_single_decay(&singleDecayedSourceType);

        if (dest->basicType == VT_ARRAY)
        {
            struct Type singleDecayedDestType = *dest;
            type_single_decay(&singleDecayedDestType);

            retVal = type_compare_allow_implicit_widening(&singleDecayedSourceType, &singleDecayedDestType);
        }
        else
        {
            retVal = type_compare_allow_implicit_widening(&singleDecayedSourceType, dest);
        }
    }
    else if (src->basicType == VT_STRUCT)
    {
        // if struct->struct, special case to compare type names (ignore any, and other types are handled in Type_CompareBasicTypeAllowImplicitWidening)
        if (dest->basicType == VT_STRUCT)
        {
            retVal = strcmp(src->nonArray.complexType.name, dest->nonArray.complexType.name);
        }
    }

    return retVal;
}

char *type_get_name(struct Type *type)
{
    const u32 SPRINT_TYPE_NAME_LENGTH = 1024;
    char *typeName = malloc(SPRINT_TYPE_NAME_LENGTH * sizeof(char));
    int len = 0;
    switch (type->basicType)
    {
    case VT_NULL:
        len = sprintf(typeName, "NOTYPE");
        break;

    case VT_ANY:
        len = sprintf(typeName, "any");
        break;

    case VT_U8:
        len = sprintf(typeName, "u8");
        break;

    case VT_U16:
        len = sprintf(typeName, "u16");
        break;

    case VT_U32:
        len = sprintf(typeName, "u32");
        break;

    case VT_U64:
        len = sprintf(typeName, "u64");
        break;

    case VT_STRUCT:
    case VT_ENUM:
        len = sprintf(typeName, "%s", type->nonArray.complexType.name);
        break;

    case VT_ARRAY:
    {
        char *arrayTypeName = type_get_name(type->array.type);
        len = sprintf(typeName, "%s[%zu]", arrayTypeName, type->array.size);
        free(arrayTypeName);
    }
    break;

    case VT_GENERIC_PARAM:
    {
        len = sprintf(typeName, "genericParam(\"%s\")", type->nonArray.complexType.name);
    }
    break;

    default:
        InternalError("Unexpected enum BASIC_TYPES value %d seen in Type_GetName!", type->basicType);
    }

    size_t pointerCounter = 0;
    for (pointerCounter = 0; pointerCounter < type->pointerLevel; pointerCounter++)
    {
        typeName[len + pointerCounter] = '*';
        len += sprintf(typeName + len, "*");
    }
    typeName[len + pointerCounter] = '\0';

    return typeName;
}

struct Type *type_duplicate(struct Type *type)
{
    struct Type *dup = malloc(sizeof(struct Type));
    memcpy(dup, type, sizeof(struct Type));
    return dup;
}

size_t type_get_size(struct Type *type, struct Scope *scope)
{
    size_t size = 0;

    if (type->pointerLevel > 0)
    {
        size = MACHINE_REGISTER_SIZE_BYTES;
        return size;
    }

    switch (type->basicType)
    {
    case VT_NULL:
        InternalError("Type_GetSize called with basic type of VT_NULL!\n");
        break;

    case VT_ANY:
        // triple check that 'any' is only ever used as a pointer type a la c's void *
        if (type->pointerLevel == 0)
        {
            char *illegalAnyTypeName = type_get_name(type);
            InternalError("Illegal 'any' type detected - %s\nSomething slipped through earlier sanity checks on use of 'any' as 'any *' or some other pointer type\n", illegalAnyTypeName);
        }
        size = sizeof(u8);
        break;

    case VT_U8:
        size = sizeof(u8);
        break;

    case VT_U16:
        size = sizeof(u16);
        break;

    case VT_U32:
        size = sizeof(u32);
        break;

    case VT_U64:
        size = sizeof(u64);
        break;

    case VT_STRUCT:
    {
        struct StructEntry *theStruct = scope_lookup_struct_by_type(scope, type);
        size = theStruct->totalSize;
    }
    break;

    case VT_ENUM:
    {
        size = sizeof(size_t);
        struct EnumEntry *theEnum = scope_lookup_enum_by_type(scope, type);
        size += theEnum->unionSize;
    }
    break;

    case VT_ARRAY:
    {
        struct Type typeRunner = *type;
        size = 1;
        while (typeRunner.basicType == VT_ARRAY)
        {
            size *= typeRunner.array.size;
            typeRunner = *typeRunner.array.type;
        }
        size *= type_get_size(&typeRunner, scope);
    }
    break;

    case VT_GENERIC_PARAM:
        InternalError("Type_GetSize called with basic type of VT_GENERIC_PARAM!\n");
        break;
    }

    return size;
}

size_t type_get_size_when_dereferenced(struct Type *type, struct Scope *scope)
{
    if (type->pointerLevel == 0)
    {
        InternalError("Type_GetSizeWhenDereferenced called with non-pointer type %s!\n", type_get_name(type));
    }
    struct Type dereferenced = *type;
    dereferenced.pointerLevel--;
    return type_get_size(&dereferenced, scope);
}

size_t type_get_size_of_array_element(struct Type *arrayType, struct Scope *scope)
{
    if (arrayType->basicType == VT_ARRAY)
    {
        struct Type element = *arrayType->array.type;
        return type_get_size(&element, scope);
    }
    if (arrayType->pointerLevel > 0)
    {
        struct Type element = *arrayType;
        element.pointerLevel--;
        return type_get_size(&element, scope);
    }

    InternalError("Type_GetSizeOfArrayElement called with non-array and non-pointer type %s!\n", type_get_name(arrayType));
}

u8 type_get_alignment(struct Type *type, struct Scope *scope)
{
    u8 alignment = 0;

    // early return for pointers (in case of pointer to undeclared struct)
    if (type->pointerLevel > 0)
    {
        alignment = align_size(sizeof(size_t));
        return alignment;
    }

    switch (type->basicType)
    {
    case VT_STRUCT:
    {
        struct StructEntry *theStruct = scope_lookup_struct_by_type(scope, type);
        Iterator *memberIterator = NULL;
        for (memberIterator = stack_bottom(theStruct->fieldLocations); iterator_gettable(memberIterator); iterator_next(memberIterator))
        {
            struct StructField *offset = iterator_get(memberIterator);
            u8 memberAlignment = type_get_alignment(&offset->variable->type, scope);
            if (memberAlignment > alignment)
            {
                alignment = memberAlignment;
            }
        }
        iterator_free(memberIterator);
    }
    break;

    case VT_ARRAY:
        alignment = align_size(type_get_size(type->array.type, scope));
        break;

    default:
        alignment = align_size(type_get_size(type, scope));
        break;
    }

    return alignment;
}

size_t scope_compute_padding_for_alignment(struct Scope *scope, struct Type *alignedType, size_t currentOffset)
{
    // calculate the number of bytes to which this member needs to be aligned
    size_t alignBytesForType = unalign_size(type_get_alignment(alignedType, scope));

    // compute how many bytes of padding we will need before this member to align it correctly
    size_t paddingRequired = 0;
    size_t bytesAfterAlignBoundary = currentOffset % alignBytesForType;
    if (bytesAfterAlignBoundary)
    {
        paddingRequired = alignBytesForType - bytesAfterAlignBoundary;
    }

    return paddingRequired;
}
