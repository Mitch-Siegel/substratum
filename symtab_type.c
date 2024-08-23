#include "symtab_type.h"
#include "enum_desc.h"
#include "log.h"
#include "struct_desc.h"
#include "symtab_function.h"
#include "symtab_trait.h"
#include "util.h"

ssize_t compare_generic_params_lists(void *paramsListDataA, void *paramsListDataB)
{
    List *paramsListA = paramsListDataA;
    List *paramsListB = paramsListDataB;

    if (paramsListA->size != paramsListB->size)
    {
        return (ssize_t)paramsListA->size - (ssize_t)paramsListB->size;
    }

    Iterator *paramIterA = list_begin(paramsListA);
    Iterator *paramIterB = list_begin(paramsListB);
    while (iterator_gettable(paramIterA) && iterator_gettable(paramIterB))
    {
        struct Type *paramTypeA = iterator_get(paramIterA);
        struct Type *paramTypeB = iterator_get(paramIterB);

        ssize_t cmpVal = type_compare(paramTypeA, paramTypeB);
        if (cmpVal != 0)
        {
            iterator_free(paramIterA);
            iterator_free(paramIterB);
            return cmpVal;
        }

        iterator_next(paramIterA);
        iterator_next(paramIterB);
    }
    iterator_free(paramIterA);
    iterator_free(paramIterB);

    return 0;
}

size_t hash_generic_params_list(void *paramsListData)
{
    List *paramsList = paramsListData;
    ssize_t hash = 0;
    Iterator *paramIter = NULL;
    for (paramIter = list_begin(paramsList); iterator_gettable(paramIter); iterator_next(paramIter))
    {
        hash <<= 1;
        hash ^= type_hash(iterator_get(paramIter)) + 1;
    }
    iterator_free(paramIter);

    return hash;
}

struct TypeEntry *type_entry_new(struct Scope *parentScope,
                                 enum TYPE_PERMUTATION permutation,
                                 struct Type type,
                                 enum GENERIC_TYPE genericType,
                                 List *genericParamNames)
{
    struct TypeEntry *wipType = malloc(sizeof(struct TypeEntry));
    memset(wipType, 0, sizeof(struct TypeEntry));
    wipType->parentScope = parentScope;
    wipType->permutation = permutation;
    wipType->type = type;
    wipType->traits = set_new(NULL, trait_entry_compare);
    wipType->implementedByName = hash_table_new(free, NULL, (ssize_t(*)(void *, void *))strcmp, hash_string, 10);

    wipType->name = type_get_name(&type);
    wipType->implemented = scope_new(parentScope, wipType->name, NULL, NULL);

    if ((genericParamNames != NULL) && (genericType != G_BASE))
    {
        InternalError("Generic struct %s has parameters but is not enumerated to be a G_BASE", type_get_name(&type));
    }

    wipType->genericType = genericType;
    wipType->generic.base.paramNames = genericParamNames;

    if (genericType == G_BASE)
    {
        wipType->generic.base.instances = hash_table_new((void (*)(void *))list_free, (void (*)(void *))struct_desc_free, compare_generic_params_lists, hash_generic_params_list, 100);
    }

    return wipType;
}

struct TypeEntry *type_entry_new_primitive(struct Scope *parentScope, enum BASIC_TYPES basicType)
{
    struct Type primitiveType = {0};
    type_init(&primitiveType);
    type_set_basic_type(&primitiveType, basicType, NULL, 0);
    struct TypeEntry *wipPrimitive = type_entry_new(parentScope, TP_PRIMITIVE, primitiveType, G_NONE, NULL);
    return wipPrimitive;
}

struct TypeEntry *type_entry_new_struct(char *name, struct Scope *parentScope, enum GENERIC_TYPE genericType, List *genericParamNames)
{
    struct Type structType = {0};
    type_init(&structType);
    type_set_basic_type(&structType, VT_STRUCT, name, 0);
    struct TypeEntry *wipStruct = type_entry_new(parentScope, TP_STRUCT, structType, genericType, genericParamNames);
    wipStruct->data.asStruct = struct_desc_new(parentScope, name);
    return wipStruct;
}

struct TypeEntry *type_entry_new_enum(char *name, struct Scope *parentScope, enum GENERIC_TYPE genericType, List *genericParamNames)
{
    struct Type enumType = {0};
    type_init(&enumType);
    type_set_basic_type(&enumType, VT_ENUM, name, 0);
    struct TypeEntry *wipEnum = type_entry_new(parentScope, TP_ENUM, enumType, genericType, genericParamNames);
    wipEnum->data.asEnum = enum_desc_new(name, parentScope);
    return wipEnum;
}

void type_entry_free(struct TypeEntry *entry)
{
    set_free(entry->traits);
    scope_free(entry->implemented);
    hash_table_free(entry->implementedByName);
    free(entry->name);

    switch (entry->permutation)
    {
    case TP_PRIMITIVE:
        break;

    case TP_STRUCT:
        struct_desc_free(entry->data.asStruct);
        break;

    case TP_ENUM:
        enum_desc_free(entry->data.asEnum);
        break;
    }

    switch (entry->genericType)
    {
    case G_BASE:
        list_free(entry->generic.base.paramNames);
        hash_table_free(entry->generic.base.instances);
        break;

    case G_INSTANCE:
    case G_NONE:
        break;
    }

    free(entry);
}

void type_entry_check_implemented_access(struct TypeEntry *theType,
                                         struct Ast *nameTree,
                                         struct Scope *accessedFromScope,
                                         char *whatAccessingCalled)
{
    // if the scope from which we are accessing:
    // 1. is a function scope
    // 2. the function is implemented for the struct
    // 3. the struct is the same as the one we are accessing
    // always allow access because private access is allowed
    if ((accessedFromScope->parentFunction->implementedFor != NULL) &&
        (accessedFromScope->parentFunction->implementedFor->permutation == TP_STRUCT) &&
        (type_compare(&theType->type, &accessedFromScope->parentFunction->implementedFor->type) == 0))
    {
        return;
    }

    struct ScopeMember *accessed = scope_lookup(theType->implemented, nameTree->value, E_FUNCTION);
    if (accessed == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call nonexistent %s %s of type %s", whatAccessingCalled, nameTree->value, theType->name);
    }

    switch (accessed->accessibility)
    {
    // nothing to check if public
    case A_PUBLIC:
        break;

    case A_PRIVATE:
        // check if the scope at which we are accessing is a subscope of (or identical to) the struct's scope
        do
        {
            if (accessedFromScope->parentScope == theType->parentScope)
            {
                break;
            }
            accessedFromScope = accessedFromScope->parentScope;
        } while (accessedFromScope->parentScope != NULL);

        if (accessedFromScope == NULL)
        {
            log_tree(LOG_FATAL, nameTree, "%s %s of %s has access specifier private - not accessible from this scope!", whatAccessingCalled, nameTree->value, theType->name);
        }
        break;
    }
}

void type_entry_add_implemented(struct TypeEntry *entry, struct FunctionEntry *implemented, enum ACCESS accessibility)
{
    struct ScopeMember *existing = scope_lookup(entry->implemented, implemented->name, E_FUNCTION);
    if (existing != NULL)
    {
        char *signature = sprint_function_signature(implemented);
        InternalError("Type %s already implements %s!", entry->name, signature);
        function_entry_print(implemented, false, 0, stderr);
    }

    scope_insert(entry->parentScope, implemented->name, implemented, E_FUNCTION, accessibility);
}

void type_entry_add_trait(struct TypeEntry *entry, struct TraitEntry *trait)
{
    // TODO: sanity check that all trait functions are implemented?
    set_insert(entry->traits, trait);
}

struct TraitEntry *type_entry_lookup_trait(struct TypeEntry *typeEntry, char *name)
{
    struct TraitEntry dummyTrait = {0};
    dummyTrait.name = name;
    return set_find(typeEntry->traits, &dummyTrait);
}

struct FunctionEntry *type_entry_lookup_implemented(struct TypeEntry *typeEntry, struct Scope *scope, struct Ast *nameTree)
{
    struct ScopeMember *implementedMember = scope_lookup(typeEntry->implemented, nameTree->value, E_FUNCTION);
    if (implementedMember == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call nonexistent implemented function %s %s of type %s", nameTree->value, typeEntry->name);
    }

    struct FunctionEntry *implementedFunction = implementedMember->entry;

    type_entry_check_implemented_access(typeEntry, nameTree, scope, "Implemented function");

    return implementedFunction;
}

struct TypeEntry *struct_type_entry_clone_generic_base_as_instance(struct TypeEntry *toClone, char *name)
{
    if ((toClone->genericType != G_BASE) || (toClone->permutation != TP_STRUCT))
    {
        InternalError("Attempt to clone non-base or non-struct %s for struct instance creation", toClone->name);
    }

    struct StructDesc *clonedStruct = struct_desc_clone(toClone->data.asStruct, name);

    struct TypeEntry *clonedTypeEntry = type_entry_new_struct(name, toClone->parentScope, G_INSTANCE, toClone->generic.base.paramNames);
    clonedTypeEntry->data.asStruct = clonedStruct;

    return clonedTypeEntry;
}

struct TypeEntry *enum_type_entry_clone_generic_base_as_instance(struct TypeEntry *toClone, char *name)
{
    if ((toClone->genericType != G_BASE) || (toClone->permutation != TP_ENUM))
    {
        InternalError("Attempt to clone non-base or non-enum %s for enum instance creation", toClone->name);
    }

    struct EnumDesc *clonedEnum = enum_desc_clone(toClone->data.asEnum, name);

    struct TypeEntry *clonedTypeEntry = type_entry_new_enum(name, toClone->parentScope, G_INSTANCE, toClone->generic.base.paramNames);
    clonedTypeEntry->data.asEnum = clonedEnum;

    return clonedTypeEntry;
}

struct TypeEntry *type_entry_clone_generic_base_as_instance(struct TypeEntry *toClone, char *name)
{
    if (toClone->genericType != G_BASE)
    {
        InternalError("Attempt to clone non-base generic struct %s for instance creation", toClone->name);
    }

    struct TypeEntry *clonedTypeEntry = NULL;

    switch (toClone->permutation)
    {
    case TP_PRIMITIVE:
        InternalError("Attempt to clone primitive type %s as generic instance", toClone->name);
        break;

    case TP_STRUCT:
        clonedTypeEntry = struct_type_entry_clone_generic_base_as_instance(toClone, name);
        break;

    case TP_ENUM:
        clonedTypeEntry = enum_type_entry_clone_generic_base_as_instance(toClone, name);
        break;
    }

    return clonedTypeEntry;
}

void type_resolve_capital_self(struct TypeEntry *theType)
{
    InternalError("type_resolve_capital_self not yet implemented");
}

struct FunctionEntry *type_entry_lookup_method(struct TypeEntry *typeEntry,
                                               struct Ast *nameTree,
                                               struct Scope *scope)
{
    struct FunctionEntry *returnedMethod = NULL;

    HashTableEntry *lookedUp = hash_table_find(typeEntry->implementedByName, nameTree->value);
    struct FunctionEntry *associatedFunction = lookedUp->value;

    if (associatedFunction == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call nonexistent method %s.%s\n", typeEntry->name, nameTree->value);
    }

    type_entry_check_implemented_access(typeEntry, nameTree, scope, "Method");

    if (returnedMethod->isMethod)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call associated function %s::%s as an method!\n", typeEntry->name, nameTree->value);
    }

    return returnedMethod;
}

struct FunctionEntry *type_entry_lookup_associated_function(struct TypeEntry *typeEntry,
                                                            struct Ast *nameTree,
                                                            struct Scope *scope)
{
    struct FunctionEntry *returnedAssociated = NULL;

    HashTableEntry *lookedUp = hash_table_find(typeEntry->implementedByName, nameTree->value);
    struct FunctionEntry *associatedFunction = lookedUp->value;

    if (associatedFunction == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call nonexistent associated function %s::%s\n", typeEntry->name, nameTree->value);
    }

    type_entry_check_implemented_access(typeEntry, nameTree, scope, "Associated function");

    if (returnedAssociated->isMethod)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call method %s.%s as an associated function!\n", typeEntry->name, nameTree->value);
    }

    return returnedAssociated;
}

void type_entry_resolve_generics(struct TypeEntry *instance, List *paramNames, List *paramTypes)
{
    if (instance->genericType != G_INSTANCE)
    {
        InternalError("type_entry_resolve_generics called with non-instance type %s", instance->name);
    }

    HashTable *paramsMap = hash_table_new(NULL, NULL, (ssize_t(*)(void *, void *))strcmp, hash_string, paramTypes->size);

    Iterator *paramNameIter = list_begin(paramNames);
    Iterator *paramTypeIter = list_begin(paramTypes);
    while (iterator_gettable(paramNameIter) && iterator_gettable(paramTypeIter))
    {
        char *paramName = iterator_get(paramNameIter);
        struct Type *paramType = iterator_get(paramTypeIter);

        char *paramTypeName = type_get_name(paramType);
        log(LOG_DEBUG, "Map \"%s\"->%s for resolution of generic %s", paramName, paramTypeName, instance->name);
        free(paramTypeName);

        hash_table_insert(paramsMap, paramName, paramType);

        iterator_next(paramNameIter);
        iterator_next(paramTypeIter);
    }

    if (iterator_gettable(paramNameIter) != iterator_gettable(paramTypeIter))
    {
        InternalError("Iteration error when generating mapping from param names to param types for generic resolution of %s", instance->name);
    }
    iterator_free(paramNameIter);
    iterator_free(paramTypeIter);

    switch (instance->permutation)
    {
    case TP_PRIMITIVE:
        InternalError("Attempt to resolve generics on primitive type %s", instance->name);
        break;
    case TP_STRUCT:
        struct_desc_resolve_generics(instance->data.asStruct, paramsMap, instance->name, paramTypes);
        break;
    case TP_ENUM:
        enum_desc_resolve_generics(instance->data.asEnum, paramsMap, instance->name, paramTypes);
        break;
    }

    hash_table_free(paramsMap);
}

extern struct Dictionary *parseDict;
struct TypeEntry *type_entry_get_or_create_generic_instantiation(struct TypeEntry *theType, List *paramsList)
{
    if (theType->genericType != G_BASE)
    {
        InternalError("type_entry_get_or_create_generic_instantiation called on non-generic-base type %s", theType->name);
    }

    if (paramsList == NULL)
    {
        InternalError("type_entry_get_or_create_generic_instantiation called with NULL paramsList");
    }

    if (theType->generic.base.paramNames->size != paramsList->size)
    {
        List *expectedParams = theType->generic.base.paramNames;
        char *expectedParamsStr = sprint_generic_param_names(expectedParams);
        InternalError("generic struct %s<%s> (%zu parameter names) instantiated with %zu params", theType->name, expectedParamsStr, expectedParams->size, paramsList->size);
    }

    struct TypeEntry *instance = hash_table_find(theType->generic.base.instances, paramsList);

    if (instance == NULL)
    {
        char *paramStr = sprint_generic_params(paramsList);
        log(LOG_DEBUG, "No instance of %s<%s> exists - creating", theType->name, paramStr);
        free(paramStr);

        instance = type_entry_clone_generic_base_as_instance(theType, theType->name);

        instance->generic.instance.parameters = paramsList;

        type_entry_resolve_generics(instance, theType->generic.base.paramNames, paramsList);

        hash_table_insert(theType->generic.base.instances, paramsList, instance);
    }

    return instance;
}

char *sprint_generic_param_names(List *paramNames)
{
    char *str = NULL;
    size_t len = 1;
    Iterator *nameIter = NULL;
    for (nameIter = list_begin(paramNames); iterator_gettable(nameIter); iterator_next(nameIter))
    {
        char *paramName = iterator_get(nameIter);
        len += strlen(paramName);
        if (str == NULL)
        {
            str = strdup(paramName);
        }
        else
        {
            len += 2;
            str = realloc(str, len);
            strncat(str, ", ", len);
            strncat(str, paramName, len);
        }
    }
    iterator_free(nameIter);

    return str;
}

char *sprint_generic_params(List *params)
{
    char *str = NULL;
    size_t len = 1;
    Iterator *paramIter = NULL;
    for (paramIter = list_begin(params); iterator_gettable(paramIter); iterator_next(paramIter))
    {
        struct Type *param = iterator_get(paramIter);
        char *paramStr = type_get_name(param);
        len += strlen(paramStr);
        if (str == NULL)
        {
            str = paramStr;
        }
        else
        {
            len += 2;
            str = realloc(str, len);
            strncat(str, ", ", len);
            strncat(str, paramStr, len);
            free(paramStr);
        }
    }
    iterator_free(paramIter);

    return str;
}

void type_entry_print(struct TypeEntry *theType, bool printTac, size_t depth, FILE *outFile)
{
    for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
    {
        fprintf(outFile, "\t");
    }

    if (theType->genericType != G_BASE)
    {
        for (size_t j = 0; j < depth; j++)
        {
            fprintf(outFile, "\t");
        }
        fprintf(outFile, "  - Size: %zu bytes\n", type_get_size(&theType->type, theType->parentScope));
    }
    switch (theType->permutation)
    {
    case TP_PRIMITIVE:
        fprintf(outFile, "Primitive type %s\n", theType->name);
        break;

    case TP_STRUCT:
        fprintf(outFile, "Struct type %s\n", theType->name);
        break;

    case TP_ENUM:
        fprintf(outFile, "Enum type %s\n", theType->name);
        break;
    }

    if (theType->genericType == G_BASE)
    {
        char *paramNames = sprint_generic_param_names(theType->generic.base.paramNames);
        fprintf(outFile, "<%s> (Generic Base)\n", paramNames);
        free(paramNames);
    }
    else if (theType->genericType == G_INSTANCE)
    {
        char *paramTypes = sprint_generic_params(theType->generic.instance.parameters);
        fprintf(outFile, "<%s> (Generic Instance)", paramTypes);
        free(paramTypes);
    }

    switch (theType->permutation)
    {
    case TP_PRIMITIVE:
        fprintf(outFile, "Primitive type %s\n", theType->name);
        break;

    case TP_STRUCT:
        struct_desc_print(theType->data.asStruct, depth, outFile);
        break;

    case TP_ENUM:
        enum_desc_print(theType->data.asEnum, depth, outFile);
        break;
    }

    for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
    {
        fprintf(outFile, "\t");
    }
    fprintf(outFile, "Implements %zu traits:\n", theType->traits->size);

    Iterator *traitIter = NULL;
    for (traitIter = set_begin(theType->traits); iterator_gettable(traitIter); iterator_next(traitIter))
    {
        struct TraitEntry *trait = iterator_get(traitIter);
        for (size_t depthPrint = 0; depthPrint < depth + 1; depthPrint++)
        {
            fprintf(outFile, "\t");
        }
        fprintf(outFile, "Trait %s\n", trait->name);
    }
    iterator_free(traitIter);

    for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
    {
        fprintf(outFile, "\t");
    }
    fprintf(outFile, "%zu implemented functions:\n", theType->implemented->entries->size);

    scope_print(theType->implemented, outFile, depth + 1, printTac);
}
