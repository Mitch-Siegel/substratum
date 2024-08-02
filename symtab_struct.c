#include "symtab_struct.h"

#include "log.h"
#include "symtab_function.h"
#include "symtab_scope.h"
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
            return cmpVal;
        }

        iterator_next(paramIterA);
        iterator_next(paramIterB);
    }

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

    return hash;
}

struct StructEntry *struct_entry_new(struct Scope *parentScope,
                                     char *name,
                                     List *genericParams)
{
    struct StructEntry *wipStruct = malloc(sizeof(struct StructEntry));
    wipStruct->name = name;
    wipStruct->genericParameters = genericParams;
    if (genericParams != NULL)
    {
        wipStruct->genericInstantiations = hash_table_new((void (*)(void *))list_free, (void (*)(void *))struct_entry_free, compare_generic_params_lists, hash_generic_params_list, 100);
    }
    else
    {
        wipStruct->genericInstantiations = NULL;
    }

    wipStruct->members = scope_new(parentScope, name, NULL, wipStruct);
    wipStruct->fieldLocations = stack_new(free);
    wipStruct->totalSize = 0;

    return wipStruct;
}

void struct_entry_free(struct StructEntry *theStruct)
{
    scope_free(theStruct->members);
    stack_free(theStruct->fieldLocations);
    if (theStruct->genericParameters != NULL)
    {
        list_free(theStruct->genericParameters);
    }
    free(theStruct);
}

struct StructEntry *struct_entry_clone(struct StructEntry *toClone, char *name)
{
    List *duplicateGenericParams = list_new((void (*)(void *))type_free, NULL);
    Iterator *paramIter = NULL;
    for (paramIter = list_begin(toClone->genericParameters); iterator_gettable(paramIter); iterator_next(paramIter))
    {
        list_append(duplicateGenericParams, strdup(iterator_get(paramIter)));
    }
    struct StructEntry *cloned = struct_entry_new(toClone->members->parentScope, name, duplicateGenericParams);

    scope_clone_to(cloned->members, toClone->members);

    scope_print(cloned->members, stderr, 0, 1);

    return cloned;
}

void struct_add_field(struct StructEntry *memberOf,
                      struct VariableEntry *variable)
{

    struct StructField *newMemberLocation = malloc(sizeof(struct StructField));

    newMemberLocation->offset = -1;
    newMemberLocation->variable = variable;

    stack_push(memberOf->fieldLocations, newMemberLocation);
}

void struct_assign_offsets_to_fields(struct StructEntry *theStruct)
{
    Iterator *fieldIter = NULL;
    for (fieldIter = stack_bottom(theStruct->fieldLocations); iterator_gettable(fieldIter); iterator_next(fieldIter))
    {
        struct StructField *handledField = iterator_get(fieldIter);
        // add the padding to the total size of the struct
        theStruct->totalSize += scope_compute_padding_for_alignment(theStruct->members, &handledField->variable->type, theStruct->totalSize);

        // place the new member at the (now aligned) current max size of the struct
        if (theStruct->totalSize > I64_MAX)
        {
            // TODO: implementation dependent size of size_t
            InternalError("Struct %s has size too large (%zd bytes)!", theStruct->name, theStruct->totalSize);
        }
        handledField->offset = (ssize_t)theStruct->totalSize;

        // add the size of the member we just added to the total size of the struct
        theStruct->totalSize += type_get_size(&handledField->variable->type, theStruct->members);
        log(LOG_DEBUG, "Assign offset %zu to member variable %s of struct %s - total struct size is now %zu", handledField->offset, handledField->variable->name, theStruct->name, theStruct->totalSize);
    }
}

// assuming we know that struct has a member with name identical to name->value, make sure we can actually access it
void struct_check_access(struct StructEntry *theStruct,
                         struct Ast *nameTree,
                         struct Scope *scope,
                         char *whatAccessingCalled)
{
    struct ScopeMember *accessed = scope_lookup(theStruct->members, nameTree->value, E_VARIABLE);
    if (accessed == NULL)
    {
        accessed = scope_lookup(theStruct->members, nameTree->value, E_FUNCTION);
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
            if (scope == theStruct->members)
            {
                break;
            }
            scope = scope->parentScope;
        } while (scope != NULL);

        if (scope == NULL)
        {
            log_tree(LOG_FATAL, nameTree, "%s %s of struct %s has access specifier private - not accessible from this scope!", whatAccessingCalled, nameTree->value, theStruct->name);
        }
        break;
    }
}

// assuming we know that struct has a field with name identical to name, make sure we can actually access it
void struct_check_access_by_name(struct StructEntry *theStruct,
                                 char *name,
                                 struct Scope *scope,
                                 char *whatAccessingCalled)
{
    struct ScopeMember *accessed = scope_lookup(theStruct->members, name, E_VARIABLE);

    switch (accessed->accessibility)
    {
    // nothing to check if public
    case A_PUBLIC:
        break;

    case A_PRIVATE:
        // check if the scope at which we are accessing is a subscope of (or identical to) the struct's scope
        do
        {
            if (scope == theStruct->members)
            {
                break;
            }
            scope = scope->parentScope;
        } while (scope != NULL);

        if (scope == NULL)
        {
            log(LOG_FATAL, "%s %s of struct %s has access specifier private - not accessible from this scope!", whatAccessingCalled, name, theStruct->name);
        }
        break;
    }
}

struct StructField *struct_lookup_field(struct StructEntry *theStruct,
                                        struct Ast *nameTree,
                                        struct Scope *scope)
{
    if (nameTree->type != T_IDENTIFIER)
    {
        log_tree(LOG_FATAL,
                 nameTree,
                 "Non-identifier tree %s (%s) passed to Struct_lookupOffsetOfMemberVariable!\n",
                 nameTree->value,
                 token_get_name(nameTree->type));
    }

    struct StructField *returnedField = NULL;
    Iterator *fieldIterator = NULL;
    for (fieldIterator = stack_bottom(theStruct->fieldLocations); iterator_gettable(fieldIterator); iterator_next(fieldIterator))
    {
        struct StructField *field = iterator_get(fieldIterator);
        if (!strcmp(field->variable->name, nameTree->value))
        {
            returnedField = field;
            break;
        }
    }
    iterator_free(fieldIterator);

    if (returnedField == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of nonexistent field \"%s\" in struct %s", nameTree->value, theStruct->name);
    }
    else
    {
        struct_check_access(theStruct, nameTree, scope, "Field");
    }

    return returnedField;
}

struct StructField *struct_lookup_field_by_name(struct StructEntry *theStruct,
                                                char *name,
                                                struct Scope *scope)
{
    struct StructField *returnedField = NULL;
    Iterator *fieldIterator = NULL;
    for (fieldIterator = stack_bottom(theStruct->fieldLocations); iterator_gettable(fieldIterator); iterator_next(fieldIterator))
    {
        struct StructField *field = iterator_get(fieldIterator);
        if (!strcmp(field->variable->name, name))
        {
            returnedField = field;
            break;
        }
    }
    iterator_free(fieldIterator);

    if (returnedField == NULL)
    {
        log(LOG_FATAL, "Use of nonexistent member variable %s in struct %s", name, theStruct->name);
    }
    else
    {
        struct_check_access_by_name(theStruct, name, scope, "Member");
    }

    return returnedField;
}

struct FunctionEntry *struct_looup_method(struct StructEntry *theStruct,
                                          struct Ast *name,
                                          struct Scope *scope)
{
    struct FunctionEntry *returnedMethod = NULL;

    struct ScopeMember *lookedUpEntry = scope_lookup_no_parent(theStruct->members, name->value, E_FUNCTION);

    if (lookedUpEntry == NULL)
    {
        log_tree(LOG_FATAL, name, "Attempt to call nonexistent method %s.%s\n", theStruct->name, name->value);
    }

    if (lookedUpEntry->type != E_FUNCTION)
    {
        log_tree(LOG_FATAL, name, "Attempt to call non-method member %s.%s as method!\n", theStruct->name, name->value);
    }

    returnedMethod = lookedUpEntry->entry;

    struct_check_access(theStruct, name, scope, "Method");

    if (!returnedMethod->isMethod)
    {
        log_tree(LOG_FATAL, name, "Attempt to call non-member associated function %s::%s as a method!\n", theStruct->name, name->value);
    }

    return returnedMethod;
}

struct FunctionEntry *struct_lookup_associated_function(struct StructEntry *theStruct,
                                                        struct Ast *nameTree,
                                                        struct Scope *scope)
{
    struct FunctionEntry *returendAssociated = NULL;

    struct ScopeMember *lookedUpEntry = scope_lookup_no_parent(theStruct->members, nameTree->value, E_FUNCTION);

    if (lookedUpEntry == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call nonexistent associated function %s::%s\n", theStruct->name, nameTree->value);
    }

    if (lookedUpEntry->type != E_FUNCTION)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call non-function member %s.%s as an associated function!\n", theStruct->name, nameTree->value);
    }

    returendAssociated = lookedUpEntry->entry;

    struct_check_access(theStruct, nameTree, scope, "Associated function");

    if (returendAssociated->isMethod)
    {
        log_tree(LOG_FATAL, nameTree, "Attempt to call method %s.%s as an associated function!\n", theStruct->name, nameTree->value);
    }

    return returendAssociated;
}

struct FunctionEntry *struct_lookup_method_by_string(struct StructEntry *theStruct,
                                                     char *name)
{
    struct FunctionEntry *returnedMethod = NULL;

    struct ScopeMember *lookedUpEntry = scope_lookup_no_parent(theStruct->members, name, E_FUNCTION);

    if (lookedUpEntry == NULL)
    {
        InternalError("Attempt to call nonexistent method %s.%s\n", theStruct->name, name);
    }

    if (lookedUpEntry->type != E_FUNCTION)
    {
        InternalError("Attempt to call non-method member %s.%s as method!\n", theStruct->name, name);
    }

    returnedMethod = lookedUpEntry->entry;

    if (!returnedMethod->isMethod)
    {
        InternalError("Attempt to call non-member associated function %s::%s as a method!\n", theStruct->name, name);
    }

    return returnedMethod;
}

struct FunctionEntry *struct_lookup_associated_function_by_string(struct StructEntry *theStruct,
                                                                  char *name)
{
    struct FunctionEntry *returendAssociated = NULL;

    struct ScopeMember *lookedUpEntry = scope_lookup_no_parent(theStruct->members, name, E_FUNCTION);

    if (lookedUpEntry == NULL)
    {
        log(LOG_FATAL, "Attempt to call nonexistent associated function %s::%s\n", theStruct->name, name);
    }

    if (lookedUpEntry->type != E_FUNCTION)
    {
        log(LOG_FATAL, "Attempt to call non-function member %s.%s as an associated function!\n", theStruct->name, name);
    }

    returendAssociated = lookedUpEntry->entry;

    if (returendAssociated->isMethod)
    {
        log(LOG_FATAL, "Attempt to call method %s.%s as an associated function!\n", theStruct->name, name);
    }

    return returendAssociated;
}

char *sprint_params(List *params)
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
            strlcat(str, ", ", len);
            strlcat(str, paramStr, len);
            free(paramStr);
        }
    }
    iterator_free(paramIter);

    return str;
}

struct StructEntry *struct_get_or_create_generic_instantiation(struct StructEntry *theStruct, List *paramsList)
{
    if (theStruct->genericParameters == NULL)
    {
        InternalError("struct_get_or_create_generic_instantiation called on non-generic struct %s", theStruct->name);
    }

    if (theStruct->genericParameters->size != paramsList->size)
    {
        char *expectedParams = sprint_generic_param_names(theStruct->genericParameters);
        InternalError("generic struct %s<%s> (%zu parameter names) instantiated with %zu params", theStruct->name, expectedParams, theStruct->genericParameters->size, paramsList->size);
    }

    struct StructEntry *instance = hash_table_find(theStruct->genericInstantiations, paramsList);

    if (instance == NULL)
    {
        char *paramStr = sprint_params(paramsList);
        log(LOG_DEBUG, "No instance of %s<%s> exists - creating", theStruct->name, paramStr);

        size_t instanceNameLen = strlen(theStruct->name) + strlen(paramStr) + 2;
        char *instanceName = malloc(instanceNameLen);
        instanceName[0] = '\0';
        strlcat(instanceName, theStruct->name, instanceNameLen);
        strlcat(instanceName, "_", instanceNameLen);
        strlcat(instanceName, paramStr, instanceNameLen);

        log(LOG_WARNING, "Instance name %s", instanceName);

        instance = struct_entry_clone(theStruct, instanceName);

        struct_resolve_generics(instance, paramsList);
    }

    return instance;
}

void struct_resolve_generics(struct StructEntry *theStruct, List *params)
{
    HashTable *paramsMap = hash_table_new(NULL, NULL, (ssize_t(*)(void *, void *))strcmp, hash_string, params->size);

    Iterator *paramNameIter = list_begin(theStruct->genericParameters);
    Iterator *paramTypeIter = list_begin(params);
    while (iterator_gettable(paramNameIter) && iterator_gettable(paramTypeIter))
    {
        char *paramName = iterator_get(paramNameIter);
        struct Type *paramType = iterator_get(paramTypeIter);

        char *paramTypeName = type_get_name(paramType);
        log(LOG_DEBUG, "Map \"%s\"->%s for resolution of generic %s", paramName, paramTypeName, theStruct->name);
        free(paramTypeName);

        hash_table_insert(paramsMap, paramName, paramType);

        iterator_next(paramNameIter);
        iterator_next(paramTypeIter);
    }

    if (iterator_gettable(paramNameIter) != iterator_gettable(paramTypeIter))
    {
        InternalError("Iteration error when generating mapping from param names to param types for generic resolution of %s", theStruct->name);
    }
    iterator_free(paramNameIter);
    iterator_free(paramTypeIter);
}
