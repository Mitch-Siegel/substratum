#include "symtab_scope.h"

#include "symtab_basicblock.h"
#include "symtab_class.h"
#include "symtab_function.h"
#include "symtab_variable.h"
#include "util.h"

extern struct Dictionary *parseDict;
/*
 * Scope functions
 *
 */
struct Scope *Scope_new(struct Scope *parentScope, char *name, struct FunctionEntry *parentFunction)
{
    struct Scope *wip = malloc(sizeof(struct Scope));
    wip->entries = Stack_New();

    wip->parentFunction = parentFunction;
    wip->parentScope = parentScope;
    wip->name = name;
    wip->subScopeCount = 0;
    return wip;
}

void Scope_free(struct Scope *scope)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *examinedEntry = scope->entries->data[entryIndex];
        switch (examinedEntry->type)
        {
        case e_scope:
            Scope_free(examinedEntry->entry);
            break;

        case e_function:
        {
            FunctionEntry_free(examinedEntry->entry);
        }
        break;

        case e_variable:
        case e_argument:
        {
            struct VariableEntry *theVariable = examinedEntry->entry;
            struct Type *variableType = &theVariable->type;
            if (variableType->basicType == vt_array)
            {
                struct Type typeRunner = *variableType;
                while (typeRunner.basicType == vt_array)
                {
                    if (typeRunner.array.initializeArrayTo != NULL)
                    {
                        for (size_t i = 0; i < typeRunner.array.size; i++)
                        {
                            free(typeRunner.array.initializeArrayTo[i]);
                        }
                        free(typeRunner.array.initializeArrayTo);
                    }
                    typeRunner = *typeRunner.array.type;
                }
            }
            else
            {
                if (variableType->nonArray.initializeTo != NULL)
                {
                    free(variableType->nonArray.initializeTo);
                }
            }
            free(theVariable);
        }
        break;

        case e_class:
        {
            struct ClassEntry *theClass = examinedEntry->entry;
            Scope_free(theClass->members);

            while (theClass->memberLocations->size > 0)
            {
                free(Stack_Pop(theClass->memberLocations));
            }

            Stack_Free(theClass->memberLocations);
            free(theClass);
        }
        break;

        case e_basicblock:
            BasicBlock_free(examinedEntry->entry);
            break;
        }

        free(examinedEntry);
    }
    Stack_Free(scope->entries);
    free(scope);
}

// insert a member with a given name and pointer to entry, along with info about the entry type
void Scope_insert(struct Scope *scope, char *name, void *newEntry, enum ScopeMemberType type)
{
    if (Scope_contains(scope, name))
    {
        ErrorAndExit(ERROR_INTERNAL, "Error defining symbol [%s] - name already exists!\n", name);
    }
    struct ScopeMember *wip = malloc(sizeof(struct ScopeMember));
    wip->name = name;
    wip->entry = newEntry;
    wip->type = type;
    Stack_Push(scope->entries, wip);
}

// create a new function accessible within the given scope
struct FunctionEntry *createFunction(struct Scope *parentScope, struct AST *nameTree, struct Type *returnType)
{
    struct FunctionEntry *newFunction = FunctionEntry_new(parentScope, nameTree, returnType);
    Scope_insert(parentScope, nameTree->value, newFunction, e_function);
    return newFunction;
}

// create and return a child scope of the scope provided as an argument
struct Scope *Scope_createSubScope(struct Scope *parentScope)
{
    if (parentScope->subScopeCount == U8_MAX)
    {
        ErrorAndExit(ERROR_INTERNAL, "Too many subscopes of scope %s\n", parentScope->name);
    }
    char *helpStr = malloc(2 + strlen(parentScope->name) + 1);
    sprintf(helpStr, "%02x", parentScope->subScopeCount);
    char *newScopeName = Dictionary_LookupOrInsert(parseDict, helpStr);
    free(helpStr);
    parentScope->subScopeCount++;

    struct Scope *newScope = Scope_new(parentScope, newScopeName, parentScope->parentFunction);
    newScope->parentFunction = parentScope->parentFunction;

    Scope_insert(parentScope, newScopeName, newScope, e_scope);
    return newScope;
}

size_t Scope_ComputePaddingForAlignment(struct Scope *scope, struct Type *alignedType, size_t currentOffset)
{
    // calculate the number of bytes to which this member needs to be aligned
    size_t alignBytesForType = unalignSize(getAlignmentOfType(scope, alignedType));

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

// Scope lookup functions

char Scope_contains(struct Scope *scope, char *name)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        if (!strcmp(name, ((struct ScopeMember *)scope->entries->data[entryIndex])->name))
        {
            return 1;
        }
    }
    return 0;
}

// if a member with the given name exists in this scope or any of its parents, return it
// also looks up entries from deeper scopes, but only as their mangled names specify
struct ScopeMember *Scope_lookup(struct Scope *scope, char *name)
{
    while (scope != NULL)
    {
        for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
        {
            struct ScopeMember *examinedEntry = scope->entries->data[entryIndex];
            if (!strcmp(examinedEntry->name, name))
            {
                return examinedEntry;
            }
        }
        scope = scope->parentScope;
    }
    return NULL;
}

size_t getSizeOfType(struct Scope *scope, struct Type *type)
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
        ErrorAndExit(ERROR_INTERNAL, "getSizeOfType called with basic type of vt_null!\n");
        break;

    case vt_any:
        // triple check that `any` is only ever used as a pointer type a la c's void *
        if (type->pointerLevel == 0)
        {
            char *illegalAnyTypeName = Type_GetName(type);
            ErrorAndExit(ERROR_INTERNAL, "Illegal `any` type detected - %s\nSomething slipped through earlier sanity checks on use of `any` as `any *` or some other pointer type\n", illegalAnyTypeName);
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
        size *= getSizeOfType(scope, &typeRunner);
    }
    break;
    }

    return size;
}

size_t getSizeOfDereferencedType(struct Scope *scope, struct Type *type)
{
    struct Type dereferenced = *type;

    if (dereferenced.pointerLevel == 0)
    {
        dereferenced = *dereferenced.array.type;
    }
    else
    {
        dereferenced.pointerLevel--;
    }

    return getSizeOfType(scope, &dereferenced);
}

// Return the number of bits required to align a given type
u8 getAlignmentOfType(struct Scope *scope, struct Type *type)
{
    return alignSize(getSizeOfType(scope, type));
}
