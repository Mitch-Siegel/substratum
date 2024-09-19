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

extern struct Dictionary *parseDict;
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
    wipType->implementedByName = hash_table_new(NULL, NULL, (ssize_t(*)(void *, void *))strcmp, hash_string, 10);

    wipType->baseName = type_get_name(&type);
    char *typeImplementedScopeName = malloc(strlen(wipType->baseName) + 13);
    sprintf(typeImplementedScopeName, "%s_implemented", wipType->baseName);
    wipType->implemented = scope_new(parentScope, dictionary_lookup_or_insert(parseDict, typeImplementedScopeName), NULL);
    free(typeImplementedScopeName);

    if ((genericParamNames != NULL) && (genericType != G_BASE))
    {
        InternalError("Generic %s has parameters but is not enumerated to be a G_BASE", type_get_name(&type));
    }

    wipType->genericType = genericType;
    wipType->generic.base.paramNames = genericParamNames;

    if (genericType == G_BASE)
    {
        wipType->generic.base.instances = hash_table_new((void (*)(void *))list_free, (void (*)(void *))type_entry_free, compare_generic_params_lists, hash_generic_params_list, 100);
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
    free(entry->baseName);

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

char *type_entry_name(struct TypeEntry *entry)
{
    char *name = type_get_name(&entry->type);
    return name;
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
        log_tree(LOG_FATAL, nameTree, "Attempt to call nonexistent %s %s of type %s", whatAccessingCalled, nameTree->value, theType->baseName);
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
            log_tree(LOG_FATAL, nameTree, "%s %s of %s has access specifier private - not accessible from this scope!", whatAccessingCalled, nameTree->value, theType->baseName);
        }
        break;
    }
}

void type_entry_add_implemented(struct TypeEntry *entry, struct FunctionEntry *implemented, enum ACCESS accessibility)
{
    hash_table_insert(entry->implementedByName, implemented->name, implemented);
}

void type_entry_add_trait(struct TypeEntry *entry, struct TraitEntry *trait)
{
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
        log_tree(LOG_FATAL, nameTree, "Attempt to call nonexistent implemented function %s of type %s", nameTree->value, typeEntry->baseName);
    }

    struct FunctionEntry *implementedFunction = implementedMember->entry;

    type_entry_check_implemented_access(typeEntry, nameTree, scope, "Implemented function");

    return implementedFunction;
}

struct TypeEntry *struct_type_entry_clone_generic_base_as_instance(struct TypeEntry *toClone, char *name)
{
    if ((toClone->genericType != G_BASE) || (toClone->permutation != TP_STRUCT))
    {
        InternalError("Attempt to clone non-base or non-struct %s for struct instance creation", toClone->baseName);
    }

    struct StructDesc *clonedStruct = struct_desc_clone(toClone->data.asStruct, name);

    struct TypeEntry *clonedTypeEntry = type_entry_new_struct(name, toClone->parentScope, G_INSTANCE, NULL);
    struct_desc_free(clonedTypeEntry->data.asStruct);
    clonedTypeEntry->data.asStruct = clonedStruct;

    scope_clone_to(clonedTypeEntry->implemented, toClone->implemented, clonedTypeEntry);

    Iterator *implementedIter = NULL;
    for (implementedIter = hash_table_begin(toClone->implementedByName); iterator_gettable(implementedIter); iterator_next(implementedIter))
    {
        HashTableEntry *implementedEntry = iterator_get(implementedIter);
        struct ScopeMember *implementedMember = implementedEntry->value;
        type_entry_add_implemented(clonedTypeEntry, scope_lookup(clonedTypeEntry->implemented, implementedEntry->key, E_FUNCTION)->entry, implementedMember->accessibility);
    }
    iterator_free(implementedIter);

    return clonedTypeEntry;
}

struct TypeEntry *enum_type_entry_clone_generic_base_as_instance(struct TypeEntry *toClone, char *name)
{
    if ((toClone->genericType != G_BASE) || (toClone->permutation != TP_ENUM))
    {
        InternalError("Attempt to clone non-base or non-enum %s for enum instance creation", toClone->baseName);
    }

    struct EnumDesc *clonedEnum = enum_desc_clone(toClone->data.asEnum, name);

    struct TypeEntry *clonedTypeEntry = type_entry_new_enum(name, toClone->parentScope, G_INSTANCE, NULL);
    enum_desc_free(clonedTypeEntry->data.asEnum);
    clonedTypeEntry->data.asEnum = clonedEnum;

    scope_clone_to(clonedTypeEntry->implemented, toClone->implemented, clonedTypeEntry);
    Iterator *implementedIter = NULL;
    for (implementedIter = hash_table_begin(toClone->implementedByName); iterator_gettable(implementedIter); iterator_next(implementedIter))
    {
        HashTableEntry *implementedEntry = iterator_get(implementedIter);
        struct ScopeMember *implementedMember = implementedEntry->value;
        type_entry_add_implemented(clonedTypeEntry, scope_lookup(clonedTypeEntry->implemented, implementedEntry->key, E_FUNCTION)->entry, implementedMember->accessibility);
    }

    iterator_free(implementedIter);

    return clonedTypeEntry;
}

struct TypeEntry *type_entry_clone_generic_base_as_instance(struct TypeEntry *toClone, char *name)
{
    if (toClone->genericType != G_BASE)
    {
        InternalError("Attempt to clone non-base generic struct %s for instance creation", toClone->baseName);
    }

    struct TypeEntry *clonedTypeEntry = NULL;

    switch (toClone->permutation)
    {
    case TP_PRIMITIVE:
        InternalError("Attempt to clone primitive type %s as generic instance", toClone->baseName);
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

struct FunctionEntry *type_entry_lookup_method(struct TypeEntry *typeEntry,
                                               struct Ast *nameTree,
                                               struct Scope *scope)
{
    struct FunctionEntry *method = hash_table_find(typeEntry->implementedByName, nameTree->value);
    if (method == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call nonexistent method %s.%s\n", typeEntry->baseName, nameTree->value);
    }

    type_entry_check_implemented_access(typeEntry, nameTree, scope, "Method");

    if (!method->isMethod)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call associated function %s::%s as an method!\n", typeEntry->baseName, nameTree->value);
    }

    return method;
}

struct FunctionEntry *type_entry_lookup_associated_function(struct TypeEntry *typeEntry,
                                                            struct Ast *nameTree,
                                                            struct Scope *scope)
{
    struct FunctionEntry *associatedFunction = hash_table_find(typeEntry->implementedByName, nameTree->value);

    if (associatedFunction == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call nonexistent associated function %s::%s\n", typeEntry->baseName, nameTree->value);
    }

    type_entry_check_implemented_access(typeEntry, nameTree, scope, "Associated function");

    if (associatedFunction->isMethod)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call method %s.%s as an associated function!\n", typeEntry->baseName, nameTree->value);
    }

    return associatedFunction;
}

void type_entry_resolve_capital_self(struct TypeEntry *typeEntry)
{
    char *typeName = type_entry_name(typeEntry);
    log(LOG_DEBUG, "Resolving capital self for type %s", typeName);
    free(typeName);

    scope_resolve_capital_self(typeEntry->implemented, typeEntry);
}

void type_entry_resolve_generics(struct TypeEntry *instance, List *paramNames, List *paramTypes)
{
    if (instance->genericType != G_INSTANCE)
    {
        InternalError("type_entry_resolve_generics called with non-instance type %s", instance->baseName);
    }

    HashTable *paramsMap = hash_table_new(NULL, NULL, (ssize_t(*)(void *, void *))strcmp, hash_string, paramTypes->size);

    Iterator *paramNameIter = list_begin(paramNames);
    Iterator *paramTypeIter = list_begin(paramTypes);
    while (iterator_gettable(paramNameIter) && iterator_gettable(paramTypeIter))
    {
        char *paramName = iterator_get(paramNameIter);
        struct Type *paramType = iterator_get(paramTypeIter);

        char *paramTypeName = type_get_name(paramType);
        log(LOG_DEBUG, "Map \"%s\"->%s for resolution of generic %s", paramName, paramTypeName, instance->baseName);
        free(paramTypeName);

        hash_table_insert(paramsMap, paramName, paramType);

        iterator_next(paramNameIter);
        iterator_next(paramTypeIter);
    }

    if (iterator_gettable(paramNameIter) != iterator_gettable(paramTypeIter))
    {
        InternalError("Iteration error when generating mapping from param names to param types for generic resolution of %s", instance->baseName);
    }
    iterator_free(paramNameIter);
    iterator_free(paramTypeIter);

    switch (instance->permutation)
    {
    case TP_PRIMITIVE:
        InternalError("Attempt to resolve generics on primitive type %s", instance->baseName);
        break;
    case TP_STRUCT:
        struct_desc_resolve_generics(instance->data.asStruct, paramsMap, instance->baseName, paramTypes);
        break;
    case TP_ENUM:
        enum_desc_resolve_generics(instance->data.asEnum, paramsMap, instance->baseName, paramTypes);
        break;
    }

    scope_resolve_generics(instance->implemented, paramsMap, instance->baseName, paramTypes);

    hash_table_free(paramsMap);

    type_entry_resolve_capital_self(instance);
}

struct TypeEntry *type_entry_get_or_create_generic_instantiation(struct TypeEntry *baseType, List *paramsList)
{
    if (baseType->genericType != G_BASE)
    {
        InternalError("type_entry_get_or_create_generic_instantiation called on non-generic-base type %s", baseType->baseName);
    }

    if (paramsList == NULL)
    {
        InternalError("type_entry_get_or_create_generic_instantiation called with NULL paramsList");
    }

    if (baseType->generic.base.paramNames->size != paramsList->size)
    {
        List *expectedParams = baseType->generic.base.paramNames;
        char *expectedParamsStr = sprint_generic_param_names(expectedParams);
        InternalError("generic struct %s<%s> (%zu parameter names) instantiated with %zu params", baseType->baseName, expectedParamsStr, expectedParams->size, paramsList->size);
    }

    struct TypeEntry *instance = hash_table_find(baseType->generic.base.instances, paramsList);

    if (instance == NULL)
    {
        char *paramStr = sprint_generic_params(paramsList);
        log(LOG_DEBUG, "No instance of %s<%s> exists - creating", baseType->baseName, paramStr);
        free(paramStr);

        instance = type_entry_clone_generic_base_as_instance(baseType, baseType->baseName);

        instance->generic.instance.parameters = paramsList;
        instance->type.nonArray.complexType.genericParams = paramsList;

        type_entry_resolve_generics(instance, baseType->generic.base.paramNames, paramsList);

        // type_entry_resolve_capital_self(instance);

        hash_table_insert(baseType->generic.base.instances, paramsList, instance);
    }

    return instance;
}

bool type_entry_verify_trait_impl(struct Ast *implTree,
                                  struct FunctionEntry *expected,
                                  struct FunctionEntry *actual,
                                  struct TraitEntry *implementedTrait,
                                  struct TypeEntry *implementedFor,
                                  enum ACCESS accessibility,
                                  enum ACCESS expectedAccessibility)
{
    bool incorrect = false;
    if (accessibility != expectedAccessibility)
    {
        incorrect = true;
        char *signature = sprint_function_signature(actual);
        switch (expectedAccessibility)
        {
        case A_PRIVATE:
        {
            log_tree(LOG_WARNING, implTree, "Function %s of trait %s is public in implementation for type %s", signature, implementedTrait->name, implementedFor->baseName);
        }
        break;
        case A_PUBLIC:
        {
            log_tree(LOG_WARNING, implTree, "Public function %s of trait %s is not public in implementation for type %s", signature, implementedTrait->name, implementedFor->baseName);
        }
        break;
        }
        free(signature);
    }
    else if (function_entry_compare(expected, actual) != 0)
    {
        incorrect = true;
        log_tree(LOG_WARNING, implTree, "Signature of function %s in implementation of trait %s for type %s doesn't match expected signature (%s)",
                 sprint_function_signature(actual), implementedTrait->name, implementedFor->baseName, sprint_function_signature(expected));
    }

    return incorrect;
}

void type_entry_verify_trait(struct Ast *implTree,
                             struct TypeEntry *implementedFor,
                             struct TraitEntry *implementedTrait,
                             Set *implementedPrivate,
                             Set *implementedPublic)
{
    Set *unImplementedPrivate = set_new(NULL, function_entry_compare);
    Set *unImplementedPublic = set_new(NULL, function_entry_compare);

    Iterator *expectedIter = NULL;
    bool incorrect = false;

    for (expectedIter = set_begin(implementedTrait->private); iterator_gettable(expectedIter); iterator_next(expectedIter))
    {
        struct FunctionEntry *expected = iterator_get(expectedIter);
        struct ScopeMember *actualEntry = scope_lookup(implementedFor->implemented, expected->name, E_FUNCTION);
        if (actualEntry == NULL)
        {
            set_insert(unImplementedPublic, expected);
        }
        else
        {
            if (set_find(implementedPrivate, actualEntry->entry) == NULL)
            {
                char *signature = sprint_function_signature(expected);
                log_tree(LOG_WARNING, implTree, "Private function %s of trait %s is not private in implementation for type %s", signature, implementedTrait->name, implementedFor->baseName);
            }
            set_remove(implementedPrivate, actualEntry->entry);
            incorrect |= type_entry_verify_trait_impl(implTree, expected, actualEntry->entry, implementedTrait, implementedFor, actualEntry->accessibility, A_PRIVATE);
        }
    }
    iterator_free(expectedIter);

    for (expectedIter = set_begin(implementedTrait->public); iterator_gettable(expectedIter); iterator_next(expectedIter))
    {
        struct FunctionEntry *expected = iterator_get(expectedIter);
        struct ScopeMember *actualEntry = scope_lookup(implementedFor->implemented, expected->name, E_FUNCTION);
        if (actualEntry == NULL)
        {
            set_insert(unImplementedPublic, expected);
        }
        else
        {
            if (set_find(implementedPublic, actualEntry->entry) == NULL)
            {
                char *signature = sprint_function_signature(expected);
                log_tree(LOG_WARNING, implTree, "Public function %s of trait %s is not public in implementation for type %s", signature, implementedTrait->name, implementedFor->baseName);
            }
            set_remove(implementedPublic, actualEntry->entry);
            incorrect |= type_entry_verify_trait_impl(implTree, expected, actualEntry->entry, implementedTrait, implementedFor, actualEntry->accessibility, A_PUBLIC);
        }
    }
    iterator_free(expectedIter);

    if (incorrect)
    {
        log_tree(LOG_FATAL, implTree, "Trait %s not correctly implemented for type %s\n", implementedTrait->name, implementedFor->baseName);
    }

    if ((unImplementedPrivate->size > 0) || (unImplementedPublic->size > 0))
    {
        Iterator *unImplementedIter = NULL;
        for (unImplementedIter = set_begin(unImplementedPrivate); iterator_gettable(unImplementedIter); iterator_next(unImplementedIter))
        {
            struct FunctionEntry *unImplemented = iterator_get(unImplementedIter);
            log_tree(LOG_WARNING, implTree, "%s not implemented for trait %s", unImplemented->name, implementedTrait->name);
        }
        iterator_free(unImplementedIter);
        for (unImplementedIter = set_begin(unImplementedPublic); iterator_gettable(unImplementedIter); iterator_next(unImplementedIter))
        {
            struct FunctionEntry *unImplemented = iterator_get(unImplementedIter);
            log_tree(LOG_WARNING, implTree, "public %s not implemented for trait %s", unImplemented->name, implementedTrait->name);
        }

        log_tree(LOG_FATAL, implTree, "Trait %s not fully implemented for type %s\n", implementedTrait->name, implementedFor->baseName);
    }

    // check for public functions implemented that are not part of the trait - this is an error
    // it makes sense that trait implementations may require extra private functions,
    // but exposing additional public functions should be disallowed to avoid confusion
    if (implementedPublic->size > 0)
    {
        char *extraPublicSignatures = NULL;

        Iterator *extraIter = NULL;
        for (extraIter = set_begin(implementedPublic); iterator_gettable(extraIter); iterator_next(extraIter))
        {
            struct FunctionEntry *extraPublic = iterator_get(extraIter);
            char *extraSignature = sprint_function_signature(extraPublic);
            if (extraPublicSignatures == NULL)
            {
                extraPublicSignatures = malloc(strlen(extraSignature) + strlen("public ") + 3);
                strcpy(extraPublicSignatures, "public ");
                strcat(extraPublicSignatures, extraSignature);
            }
            else
            {
                extraPublicSignatures = realloc(extraPublicSignatures, strlen(extraPublicSignatures) + strlen(extraSignature) + strlen("public ") + 3);
                strcat(extraPublicSignatures, "\n");
                strcat(extraPublicSignatures, "public ");
                strcat(extraPublicSignatures, extraSignature);
            }
        }

        log_tree(LOG_FATAL, implTree, "Implementation of trait %s for %s includes %zu public functions not part of trait:\n%s",
                 implementedTrait->name, implementedFor->baseName, implementedPublic->size, extraPublicSignatures);
    }

    set_free(implementedPrivate);
    set_free(implementedPublic);

    set_free(unImplementedPrivate);
    set_free(unImplementedPublic);
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

    for (size_t j = 0; j < depth; j++)
    {
        fprintf(outFile, "\t");
    }
    switch (theType->permutation)
    {
    case TP_PRIMITIVE:
        fprintf(outFile, "Primitive type %s", theType->baseName);
        break;

    case TP_STRUCT:
        fprintf(outFile, "Struct type %s", theType->baseName);
        break;

    case TP_ENUM:
        fprintf(outFile, "Enum type %s", theType->baseName);
        break;
    }

    switch (theType->genericType)
    {
    case G_NONE:
        fprintf(outFile, "\n");
        break;

    case G_BASE:
    {
        char *paramNames = sprint_generic_param_names(theType->generic.base.paramNames);
        fprintf(outFile, "<%s> (Generic Base)\n", paramNames);
        free(paramNames);
    }
    break;

    case G_INSTANCE:
    {
        char *paramTypes = sprint_generic_params(theType->generic.instance.parameters);
        fprintf(outFile, "<%s> (Generic Instance)\n", paramTypes);
        free(paramTypes);
    }
    break;
    }

    switch (theType->permutation)
    {
    case TP_PRIMITIVE:
        fprintf(outFile, "Primitive type %s\n", theType->baseName);
        break;

    case TP_STRUCT:
        struct_desc_print(theType->data.asStruct, depth + 1, outFile);
        break;

    case TP_ENUM:
        enum_desc_print(theType->data.asEnum, depth + 1, outFile);
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

    if (theType->genericType == G_BASE)
    {
        for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
        {
            fprintf(outFile, "\t");
        }
        fprintf(outFile, "%zu instances\n", theType->generic.base.instances->size);
        Iterator *instanceIter = NULL;
        for (instanceIter = hash_table_begin(theType->generic.base.instances); iterator_gettable(instanceIter); iterator_next(instanceIter))
        {
            HashTableEntry *instanceEntry = iterator_get(instanceIter);
            struct TypeEntry *instance = instanceEntry->value;
            type_entry_print(instance, printTac, depth + 2, outFile);
        }
        iterator_free(instanceIter);
    }
}
