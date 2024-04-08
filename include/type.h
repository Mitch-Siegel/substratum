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
    vt_class
};

struct Type
{
    enum basicTypes basicType;
    int indirectionLevel;
    size_t arraySize;
    union
    {
        char *initializeTo;
        char **initializeArrayTo;
    };
    struct classType
    {
        char *name;
    } classType;
};

int Type_Compare(struct Type *typeA, struct Type *typeB);

// return 0 if 'a' is the same type as 'b', or if it can implicitly be widened to become equivalent
int Type_CompareAllowImplicitWidening(struct Type *typeA, struct Type *typeB);

char *Type_GetName(struct Type *type);

#endif
