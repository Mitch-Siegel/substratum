#ifndef TYPE_H
#define TYPE_H

#include "substratum_defs.h"

enum basicTypes
{
    vt_null, // type information describing no type at all (only results from declaration of functions with no return)
    vt_any,  // type information describing an pointer to indistinct type (a la c's void pointer, but to avoid use of the 'void' keyword, must have indirection level > 0)
    vt_u8,
    vt_u16,
    vt_u32,
    vt_u64,
    vt_class,
    vt_array
};

struct Type
{
    enum basicTypes basicType;
    size_t pointerLevel;
    union
    {
        struct
        {
            u8 *initializeTo;
            struct complexType
            {
                char *name;
            } complexType;
        } nonArray;
        struct
        {
            size_t size;
            struct Type *type;
            void **initializeArrayTo;
        } array;
    };
};

void Type_Init(struct Type *type);

struct Type *Type_New();

void Type_SetBasicType(struct Type *type, enum basicTypes basicType, char *complexTypeName, size_t pointerLevel);

size_t Type_GetIndirectionLevel(struct Type *type);

void Type_DecayArrays(struct Type *type);

int Type_Compare(struct Type *typeA, struct Type *typeB);

// return 0 if 'a' is the same type as 'b', or if it can implicitly be widened to become equivalent
int Type_CompareAllowImplicitWidening(struct Type *typeA, struct Type *typeB);

char *Type_GetName(struct Type *type);

u8 Type_GetAlignment(struct Scope *scope, struct Type *type);

#endif
