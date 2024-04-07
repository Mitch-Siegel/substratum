#include "symtab_scope.h"

#include "util.h"
#include "symtab_basicblock.h"
#include "symtab_function.h"
#include "symtab_variable.h"
#include "symtab_class.h"

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
            if (variableType->initializeTo != NULL)
            {
                if (variableType->arraySize > 0)
                {
                    for (size_t arrayInitializeIndex = 0; arrayInitializeIndex < variableType->arraySize; arrayInitializeIndex++)
                    {
                        free(variableType->initializeArrayTo[arrayInitializeIndex]);
                    }
                    free(variableType->initializeArrayTo);
                }
                else
                {
                    free(variableType->initializeTo);
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
struct FunctionEntry *Scope_createFunction(struct Scope *parentScope, struct AST *nameTree, struct Type *returnType)
{
    struct FunctionEntry *newFunction = FunctionEntry_new(parentScope, nameTree, returnType);
    Scope_insert(parentScope, nameTree->value, newFunction, e_function);
    return newFunction;
}

// create and return a child scope of the scope provided as an argument
struct Scope *Scope_createSubScope(struct Scope *parentScope)
{
    if (parentScope->subScopeCount == 0xff)
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

int Scope_ComputePaddingForAlignment(struct Scope *scope, struct Type *alignedType, int currentOffset)
{
    // calculate the number of bytes to which this member needs to be aligned
    int alignBytesForType = unalignSize(Scope_getAlignmentOfType(scope, alignedType));

    // compute how many bytes of padding we will need before this member to align it correctly
    int paddingRequired = 0;
    int bytesAfterAlignBoundary = currentOffset % alignBytesForType;
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

size_t Scope_getSizeOfType(struct Scope *scope, struct Type *type)
{
    size_t size = 0;

    if (type->indirectionLevel > 0)
    {
        size = MACHINE_REGISTER_SIZE_BYTES;
        if (type->arraySize == 0)
        {
            return size;
        }
    }

    switch (type->basicType)
    {
    case vt_null:
        ErrorAndExit(ERROR_INTERNAL, "Scope_getSizeOfType called with basic type of vt_null!\n");
        break;

    case vt_any:
        // triple check that `any` is only ever used as a pointer type a la c's void *
        if ((type->indirectionLevel == 0) || (type->arraySize > 0))
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
        struct ClassEntry *class = Scope_lookupClassByType(scope, type);
        size = class->totalSize;
    }
    break;
    }

    if (type->arraySize > 0)
    {
        if (type->indirectionLevel > 1)
        {
            size = MACHINE_REGISTER_SIZE_BYTES;
        }

        size *= type->arraySize;
    }

    return size;
}

size_t Scope_getSizeOfDereferencedType(struct Scope *scope, struct Type *type)
{
    struct Type dereferenced = *type;
    dereferenced.indirectionLevel--;

    if (dereferenced.arraySize > 0)
    {
        dereferenced.arraySize = 0;
        dereferenced.indirectionLevel++;
    }
    return Scope_getSizeOfType(scope, &dereferenced);
}

size_t Scope_getSizeOfArrayElement(struct Scope *scope, struct VariableEntry *variable)
{
    size_t size = 0;
    if (variable->type.arraySize < 1)
    {
        if (variable->type.indirectionLevel)
        {
            char *nonArrayPointerTypeName = Type_GetName(&variable->type);
            printf("Warning - variable %s with type %s used in array access!\n", variable->name, nonArrayPointerTypeName);
            free(nonArrayPointerTypeName);
            struct Type elementType = variable->type;
            elementType.indirectionLevel--;
            elementType.arraySize = 0;
            size = Scope_getSizeOfType(scope, &elementType);
        }
        else
        {
            ErrorAndExit(ERROR_INTERNAL, "Non-array variable %s passed to Scope_getSizeOfArrayElement!\n", variable->name);
        }
    }
    else
    {
        if (variable->type.indirectionLevel == 0)
        {
            struct Type elementType = variable->type;
            elementType.indirectionLevel--;
            elementType.arraySize = 0;
            size = Scope_getSizeOfType(scope, &elementType);
        }
        else
        {
            size = MACHINE_REGISTER_SIZE_BYTES;
        }
    }

    return size;
}

// Return the number of bits required to align a given type
u8 Scope_getAlignmentOfType(struct Scope *scope, struct Type *type)

{
    u8 alignBits = 0;

    // TODO: handle arrays of pointers
    if (type->indirectionLevel > 0)
    {
        alignBits = alignSize(sizeof(size_t));
        if (type->arraySize == 0)
        {
            return alignBits;
        }
    }

    switch (type->basicType)
    {
    case vt_null:
        ErrorAndExit(ERROR_INTERNAL, "Scope_getAlignmentOfType called with basic type of vt_null!\n");
        break;

    case vt_any:
        // triple check that `any` is only ever used as a pointer type a la c's void *
        if ((type->indirectionLevel == 0) || (type->arraySize > 0))
        {
            char *illegalAnyTypeName = Type_GetName(type);
            ErrorAndExit(ERROR_INTERNAL, "Illegal `any` type detected - %s\nSomething slipped through earlier sanity checks on use of `any` as `any *` or some other pointer type\n", illegalAnyTypeName);
        }
        // TODO: unreachable? indirectionlevels > 0 should always be caught above.
        alignBits = alignSize(sizeof(size_t));
        break;

    // the compiler is becoming the compilee
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
        struct ClassEntry *class = Scope_lookupClassByType(scope, type);

        for (size_t memberIndex = 0; memberIndex < class->memberLocations->size; memberIndex++)
        {
            struct ClassMemberOffset *examinedMember = (struct ClassMemberOffset *)class->memberLocations->data[memberIndex];

            u8 examinedMemberAlignment = Scope_getAlignmentOfType(scope, &examinedMember->variable->type);
            if (examinedMemberAlignment > alignBits)
            {
                alignBits = examinedMemberAlignment;
            }
        }
    }
    break;
    }

    // TODO: see above todo about handling arrays of pointers
    if (type->arraySize > 0)
    {
        if (type->indirectionLevel > 1)
        {
            alignBits = alignSize(sizeof(size_t));
        }
    }

    return alignBits;
}
