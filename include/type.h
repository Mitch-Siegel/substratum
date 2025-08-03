#ifndef TYPE_H
#define TYPE_H

#include "substratum_defs.h"

#include <mbcl/hash_table.h>
#include <mbcl/list.h>

struct Scope;
struct StructDesc;
struct TypeEntry;
struct Ast;
struct FunctionEntry;

enum BASIC_TYPES
{
    VT_NULL, // type information describing no type at all (only results from declaration of functions with no return)
    VT_ANY,  // type information describing an pointer to indistinct type (a la c's void pointer, but to avoid use of the 'void' keyword, must have indirection level > 0)
    VT_U8,
    VT_U16,
    VT_U32,
    VT_U64,
    VT_STRUCT,
    VT_ENUM,
    VT_ARRAY,
    VT_GENERIC_PARAM,
    VT_SELF,
};

struct Type
{
    enum BASIC_TYPES basicType;
    size_t pointerLevel;
    union
    {
        struct
        {
            u8 *initializeTo;
            struct complexType
            {
                char *name;
                List *genericParams; // if this is a struct which is a generic type, this will be a list of pointers to Types of the params
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

void type_init(struct Type *type);

void type_deinit(struct Type *type);

void type_free(struct Type *type);

void type_set_basic_type(struct Type *type, enum BASIC_TYPES basicType, char *complexTypeName, size_t pointerLevel);

size_t type_get_indirection_level(struct Type *type);

void type_single_decay(struct Type *type);

void type_decay_arrays(struct Type *type);

// copy a type, turning any array size > 0 into an increment of indirectionlevel
void type_copy_decay_arrays(struct Type *dest, struct Type *src);

ssize_t type_compare(struct Type *typeA, struct Type *typeB);

// Compares with handling for automatic attempt to resolve VT_SELF and re-copmare on failure of direct comparison
ssize_t type_compare_allow_self(struct Type *typeA,
                                struct FunctionEntry *functionA,
                                struct Type *typeB,
                                struct FunctionEntry *functionB);

size_t type_hash(struct Type *type);

bool type_is_object(struct Type *type);

bool type_is_array_object(struct Type *type);

bool type_is_struct_object(struct Type *type);

bool type_is_enum_object(struct Type *type);

// return 0 if 'a' is the same type as 'b', or if it can implicitly be widened to become equivalent
int type_compare_allow_implicit_widening(struct Type *src, struct Type *dest);

char *type_get_name(struct Type *type);

char *type_get_mangled_name(struct Type *type);

struct Type *type_duplicate(struct Type *type);

struct Type type_duplicate_non_pointer(struct Type *type);

// gets the byte size (not aligned) of a given type
size_t type_get_size(struct Type *type, struct Scope *scope);

size_t type_get_size_when_dereferenced(struct Type *type, struct Scope *scope);

size_t type_get_size_of_array_element(struct Type *arrayType, struct Scope *scope);

// calculate the power of 2 to which a given type needs to be aligned
u8 type_get_alignment(struct Type *type, struct Scope *scope);

bool type_is_generic(struct Type *type);

void type_try_resolve_generic(struct Type *type, HashTable *paramsMap, char *resolvedStructName, List *resolvedParams);

void type_try_resolve_vt_self_unchecked(struct Type *type, struct TypeEntry *typeEntry);

void type_try_resolve_vt_self(struct Type *type, struct TypeEntry *typeEntry);

void compare_generic_param_names(struct Ast *genericParamsTree, List *actualParamNames, List *expectedParamNames, char *genericType, char *genericName);

ssize_t compare_generic_params(List *actualParams, List *expectedParams);

#endif
