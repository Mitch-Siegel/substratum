#include "symtab_scope.h"

#include "enum_desc.h"
#include "log.h"
#include "struct_desc.h"
#include "symtab_basicblock.h"
#include "symtab_function.h"
#include "symtab_trait.h"
#include "symtab_type.h"
#include "symtab_variable.h"
#include "util.h"

extern struct Dictionary *parseDict;

ssize_t scope_member_compare(struct ScopeMember *memberA, struct ScopeMember *memberB)
{
    ssize_t cmpVal = 0;
    if (memberA->type == memberB->type)
    {
        cmpVal = strcmp(memberA->name, memberB->name);
    }
    else
    {
        cmpVal = (ssize_t)memberA->type - (ssize_t)memberB->type;
    }
    return cmpVal;
}

void scope_member_free(struct ScopeMember *member)
{
    switch (member->type)
    {
    case E_SCOPE:
        scope_free(member->entry);
        break;

    case E_FUNCTION:
        function_entry_free(member->entry);
        break;

    case E_VARIABLE:
    case E_ARGUMENT:
        variable_entry_free(member->entry);
        break;

    case E_TYPE:
        type_entry_free(member->entry);
        break;

    case E_BASICBLOCK:
        basic_block_free(member->entry);
        break;

    case E_TRAIT:
        trait_free(member->entry);
        break;
    }
    free(member);
}

/*
 * Scope functions
 *
 */
struct Scope *scope_new(struct Scope *parentScope, char *name, struct FunctionEntry *parentFunction)
{
    struct Scope *wip = malloc(sizeof(struct Scope));
    wip->entries = set_new((void (*)(void *))scope_member_free, (ssize_t(*)(void *, void *))scope_member_compare);

    wip->parentScope = parentScope;
    wip->parentFunction = parentFunction;
    wip->name = name;
    wip->subScopeCount = 0;
    return wip;
}

void scope_free(struct Scope *scope)
{
    set_free(scope->entries);
    free(scope);
}

void print_accessibility(enum ACCESS accessibility, FILE *outFile)
{
    switch (accessibility)
    {
    case A_PRIVATE:
        fprintf(outFile, " - Private");
        break;

    case A_PUBLIC:
        fprintf(outFile, " - Public");
        break;
    }
}

void scope_print_member(struct ScopeMember *toPrint, bool printTac, size_t depth, FILE *outFile)
{
    for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
    {
        fprintf(outFile, "\t");
    }

    fprintf(outFile, "%p:", toPrint);

    switch (toPrint->type)
    {
    case E_ARGUMENT:
    {
        struct VariableEntry *theArgument = toPrint->entry;
        fprintf(outFile, "> Argument: %s", toPrint->name);
        print_accessibility(toPrint->accessibility, outFile);
        fprintf(outFile, "\n");

        variable_entry_print(theArgument, outFile, depth + 1);
        for (size_t depthPrint = 0; depthPrint < depth + 1; depthPrint++)
        {
            fprintf(outFile, "\t");
        }
        fprintf(outFile, "Stack offset: %zd\n", theArgument->stackOffset);
    }
    break;

    case E_VARIABLE:
    {
        struct VariableEntry *theVariable = toPrint->entry;
        fprintf(outFile, "> Variable %s", toPrint->name);
        print_accessibility(toPrint->accessibility, outFile);
        fprintf(outFile, "\n");

        variable_entry_print(theVariable, outFile, depth + 1);
    }
    break;

    case E_TYPE:
    {
        struct TypeEntry *theType = toPrint->entry;
        fprintf(outFile, "> Type %s ", toPrint->name);
        print_accessibility(toPrint->accessibility, outFile);
        fprintf(outFile, "\n");
        type_entry_print(theType, printTac, depth + 1, outFile);
    }
    break;

    case E_FUNCTION:
    {
        struct FunctionEntry *theFunction = toPrint->entry;
        if (theFunction->implementedFor != NULL)
        {
            fprintf(outFile, "> Method %s.", theFunction->implementedFor->baseName);
        }
        else
        {
            fprintf(outFile, "> Function ");
        }
        fprintf(outFile, "%s", toPrint->name);
        print_accessibility(toPrint->accessibility, outFile);
        fprintf(outFile, "\n");
        function_entry_print(theFunction, printTac, depth + 1, outFile);
    }
    break;

    case E_SCOPE:
    {
        struct Scope *theScope = toPrint->entry;
        fprintf(outFile, "> Subscope %s\n", toPrint->name);
        scope_print(theScope, outFile, depth + 1, printTac);
    }
    break;

    case E_BASICBLOCK:
    {
        struct BasicBlock *thisBlock = toPrint->entry;
        fprintf(outFile, "> Basic Block %zu - %zu TAC lines\n", thisBlock->labelNum, thisBlock->TACList->size);
        if (printTac)
        {
            print_basic_block(thisBlock, depth + 1);
        }
    }
    break;

    case E_TRAIT:
    {
        struct TraitEntry *theTrait = toPrint->entry;
        fprintf(outFile, "> Trait %s\n", theTrait->name);
        trait_entry_print(theTrait, depth + 1, outFile);
    }
    break;
    }
}

void scope_print(struct Scope *scope, FILE *outFile, size_t depth, bool printTac)
{
    Iterator *memberIterator = NULL;
    for (memberIterator = set_begin(scope->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
    {
        struct ScopeMember *thisMember = iterator_get(memberIterator);
        scope_print_member(thisMember, printTac, depth + 1, outFile);
    }
    iterator_free(memberIterator);
}

// get all members of a given type from a scope, returning them in a stack with the first members on top
Stack *scope_get_all_members_of_type(struct Scope *scope, enum SCOPE_MEMBER_TYPE type)
{
    Stack *result = stack_new(NULL);
    Iterator *entryIter = NULL;
    for (entryIter = set_begin(scope->entries); iterator_gettable(entryIter); iterator_next(entryIter))
    {
        struct ScopeMember *thisMember = iterator_get(entryIter);
        if (thisMember->type == type)
        {
            stack_push(result, thisMember->entry);
        }
    }
    iterator_free(entryIter);
    return result;
}

void dump_indent(FILE *outFile, size_t depth)
{
    for (size_t i = 0; i < depth; i++)
    {
        fprintf(outFile, "    ");
    }
}

void dump_start_row(FILE *outFile, size_t depth, char *port)
{
    dump_indent(outFile, depth);
    if (port != NULL)
    {
        fprintf(outFile, "<tr> <td port=\"%s\">", port);
    }
    else
    {
        fprintf(outFile, "<tr> <td>");
    }
}

void dump_end_row(FILE *outFile, size_t depth)
{
    dump_indent(outFile, depth);
    fprintf(outFile, "</td> </tr>\n");
}

void dump_start_table(FILE *outFile, char *tableName, size_t *depth)
{
    fprintf(outFile, "<table border=\"0\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"4\">\n");
    (*depth)++;
    dump_indent(outFile, *depth);
    fprintf(outFile, "<tr> <td><b>%s</b></td> </tr>\n", tableName);
}

void dump_end_table(FILE *outFile, size_t *depth)
{
    (*depth)--;
    dump_indent(outFile, *depth);
    fprintf(outFile, "</table>\n");
}

char *dump_get_full_scope_name(struct Scope *scope)
{
    if (scope->parentScope == NULL)
    {
        return strdup(scope->name);
    }
    char *parentName = dump_get_full_scope_name(scope->parentScope);
    char *result = malloc(strlen(parentName) + strlen(scope->name) + 2);
    ssize_t len = sprintf(result, "%s", parentName);
    ssize_t idx = 0;
    while (scope->name[idx] != '\0')
    {
        switch (scope->name[idx])
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            result[len + idx] = 'a' + (scope->name[idx] - '0');
            break;

        default:
            result[len + idx] = scope->name[idx];
        }
        result[len + idx + 1] = '\0';
        idx++;
    }
    free(parentName);
    return result;
}

void scope_dump_arguments_dot(FILE *outFile, struct Scope *scope, size_t depth)
{
    Stack *argsToDump = scope_get_all_members_of_type(scope, E_ARGUMENT);
    if (argsToDump->size == 0)
    {
        log(LOG_DEBUG, "No arguments to dump in scope %s", scope->name);
        stack_free(argsToDump);
        return;
    }
    log(LOG_DEBUG, "Dumping arguments in scope %s", scope->name);

    dump_start_row(outFile, depth, NULL);
    dump_start_table(outFile, "Arguments", &depth);

    while (argsToDump->size > 0)
    {
        struct VariableEntry *dumped = stack_pop(argsToDump);
        dump_start_row(outFile, depth, NULL);

        char *typeName = type_get_name(&dumped->type);
        fprintf(outFile, "%s %s", typeName, dumped->name);
        free(typeName);
        dump_end_row(outFile, depth);
    }
    stack_free(argsToDump);

    dump_end_table(outFile, &depth);
    dump_end_row(outFile, depth);
}

void scope_dump_variables_dot(FILE *outFile, struct Scope *scope, size_t depth)
{
    Stack *varsToDump = scope_get_all_members_of_type(scope, E_VARIABLE);
    if (varsToDump->size == 0)
    {
        log(LOG_DEBUG, "No variables to dump in scope %s", scope->name);
        stack_free(varsToDump);
        return;
    }
    log(LOG_DEBUG, "Dumping variables in scope %s", scope->name);

    dump_start_row(outFile, depth, NULL);
    dump_start_table(outFile, "Variables", &depth);

    while (varsToDump->size > 0)
    {
        struct VariableEntry *dumped = stack_pop(varsToDump);

        dump_start_row(outFile, depth, NULL);
        char *typeName = type_get_name(&dumped->type);
        fprintf(outFile, "%s %s", typeName, dumped->name);
        free(typeName);
        dump_end_row(outFile, depth);
    }
    dump_end_table(outFile, &depth);
    dump_end_row(outFile, depth);

    stack_free(varsToDump);
}

void scope_dump_types_dot(FILE *outFile, struct Scope *scope, size_t depth)
{
    Stack *typesToDump = scope_get_all_members_of_type(scope, E_TYPE);
    if (typesToDump->size == 0)
    {
        log(LOG_DEBUG, "No types to dump in scope %s", scope->name);
        stack_free(typesToDump);
        return;
    }
    log(LOG_DEBUG, "Dumping types in scope %s", scope->name);

    dump_start_row(outFile, depth, NULL);
    dump_start_table(outFile, "Types", &depth);

    while (typesToDump->size > 0)
    {
        struct TypeEntry *dumped = stack_pop(typesToDump);
        dump_start_row(outFile, depth, dumped->baseName);
        switch (dumped->permutation)
        {
        case TP_PRIMITIVE:
            fprintf(outFile, "Primitive type %s", dumped->baseName);
            break;

        case TP_STRUCT:
            fprintf(outFile, "Struct %s", dumped->baseName);
            break;

        case TP_ENUM:
            fprintf(outFile, "Enum %s", dumped->baseName);
            break;
        }
        dump_end_row(outFile, depth);
    }

    dump_end_table(outFile, &depth);
    dump_end_row(outFile, depth);

    stack_free(typesToDump);
}

void scope_dump_traits_dot(FILE *outFile, struct Scope *scope, size_t depth)
{
    Stack *traitsToDump = scope_get_all_members_of_type(scope, E_TRAIT);
    if (traitsToDump->size == 0)
    {
        log(LOG_DEBUG, "No traits to dump in scope %s", scope->name);
        stack_free(traitsToDump);
        return;
    }
    log(LOG_DEBUG, "Dumping traits in scope %s", scope->name);

    dump_start_row(outFile, depth, NULL);
    dump_start_table(outFile, "Traits", &depth);

    while (traitsToDump->size > 0)
    {
        struct TraitEntry *dumped = stack_pop(traitsToDump);
        dump_start_row(outFile, depth, dumped->name);
        fprintf(outFile, "Trait %s", dumped->name);
        dump_end_row(outFile, depth);
    }

    dump_end_table(outFile, &depth);
    dump_end_row(outFile, depth);

    stack_free(traitsToDump);
}

void scope_dump_functions_dot(FILE *outFile, struct Scope *scope, size_t depth)
{
    Stack *functionsToDump = scope_get_all_members_of_type(scope, E_FUNCTION);
    if (functionsToDump->size == 0)
    {
        log(LOG_DEBUG, "No functions to dump in scope %s", scope->name);
        stack_free(functionsToDump);
        return;
    }
    log(LOG_DEBUG, "Dumping functions in scope %s", scope->name);

    dump_start_row(outFile, depth, NULL);
    dump_start_table(outFile, "Functions", &depth);
    while (functionsToDump->size > 0)
    {
        struct FunctionEntry *dumped = stack_pop(functionsToDump);
        dump_start_row(outFile, depth, dumped->name);
        if (dumped->returnType.basicType != VT_NULL)
        {
            char *signature = sprint_function_signature(dumped);
            fprintf(outFile, "Function %s (defined? %d)", signature, dumped->isDefined);
            free(signature);
        }
        else
        {
            fprintf(outFile, "Function %s (no return) (defined: %d)", dumped->name, dumped->isDefined);
        }
    }
    dump_end_table(outFile, &depth);
    dump_end_row(outFile, depth);

    stack_free(functionsToDump);
}

void scope_dump_basicblocks_dot(FILE *outFile, struct Scope *scope, size_t depth, bool printTac)
{
    Stack *blocksToDump = scope_get_all_members_of_type(scope, E_BASICBLOCK);

    if (blocksToDump->size == 0)
    {
        log(LOG_DEBUG, "No basic blocks to dump in scope %s", scope->name);
        stack_free(blocksToDump);
        return;
    }
    log(LOG_DEBUG, "Dumping basic blocks in scope %s", scope->name);

    while (blocksToDump->size > 0)
    {
        struct BasicBlock *dumped = stack_pop(blocksToDump);
        if (printTac)
        {
            char blockName[33];
            sprintf(blockName, "Basic Block %zu", dumped->labelNum);
            dump_start_table(outFile, blockName, &depth);
            Iterator *tacIter = list_begin(dumped->TACList);
            while (iterator_gettable(tacIter))
            {
                struct TACLine *thisTac = iterator_get(tacIter);
                dump_start_row(outFile, depth, NULL);
                char *tacString = sprint_tac_line(thisTac);
                fprintf(outFile, "%s", tacString);
                free(tacString);
                dump_end_row(outFile, depth);
                iterator_next(tacIter);
            }
            iterator_free(tacIter);
        }
        else
        {
            dump_start_row(outFile, depth, NULL);
            fprintf(outFile, "Basic Block %zu", dumped->labelNum);
            dump_end_row(outFile, depth);
        }
    }

    stack_free(blocksToDump);
}

void scope_dump_subscopes_dot(FILE *outFile, struct Scope *scope, size_t depth, bool printTac)
{
    Stack *scopesToDump = scope_get_all_members_of_type(scope, E_SCOPE);
    if (scopesToDump->size == 0)
    {
        log(LOG_DEBUG, "No subscopes to dump in scope %s", scope->name);
        stack_free(scopesToDump);
        return;
    }
    log(LOG_DEBUG, "Dumping subscopes in scope %s", scope->name);

    while (scopesToDump->size > 0)
    {
        struct Scope *dumped = stack_pop(scopesToDump);
        scope_dump_dot(outFile, dumped, depth + 1, printTac);
    }

    stack_free(scopesToDump);
}

void scope_dump_dot(FILE *outFile, struct Scope *scope, size_t depth, bool printTAC)
{
    log(LOG_DEBUG, "Dumping scope %s", scope->name);
    dump_indent(outFile, depth);
    char *fullScopeName = dump_get_full_scope_name(scope);
    fprintf(outFile, "%s [\n", fullScopeName);
    free(fullScopeName);
    dump_indent(outFile, depth);
    fprintf(outFile, "shape=plain\n");
    dump_indent(outFile, depth);
    fprintf(outFile, "label=<");

    dump_start_table(outFile, scope->name, &depth);

    scope_dump_arguments_dot(outFile, scope, depth);
    scope_dump_variables_dot(outFile, scope, depth);
    scope_dump_types_dot(outFile, scope, depth);
    scope_dump_traits_dot(outFile, scope, depth);
    scope_dump_functions_dot(outFile, scope, depth);
    scope_dump_basicblocks_dot(outFile, scope, depth, printTAC);
    // scope_dump_subscopes_dot(outFile, scope, depth, printTAC);

    dump_end_table(outFile, &depth);

    dump_indent(outFile, depth);
    fprintf(outFile, ">]\n");

    Iterator *entryIter = NULL;
    for (entryIter = set_begin(scope->entries); iterator_gettable(entryIter); iterator_next(entryIter))
    {
        struct ScopeMember *thisMember = iterator_get(entryIter);
        switch (thisMember->type)
        {
        case E_SCOPE:
            dump_indent(outFile, depth);
            char *fromScopeName = dump_get_full_scope_name(scope);
            char *toScopeName = dump_get_full_scope_name(thisMember->entry);
            fprintf(outFile, "%s->%s\n", fromScopeName, toScopeName);
            free(fromScopeName);
            free(toScopeName);
            scope_dump_dot(outFile, thisMember->entry, depth + 1, printTAC);
            break;

        case E_FUNCTION:
        {
            struct FunctionEntry *thisFunction = thisMember->entry;
            scope_dump_dot(outFile, thisFunction->mainScope, depth + 1, printTAC);
            dump_indent(outFile, depth);
            char *fromScopeName = dump_get_full_scope_name(scope);
            char *toScopeName = dump_get_full_scope_name(thisFunction->mainScope);
            fprintf(outFile, "%s->%s\n", fromScopeName, toScopeName);
            free(fromScopeName);
            free(toScopeName);
        }
        break;

        case E_BASICBLOCK:
        case E_VARIABLE:
        case E_ARGUMENT:
        case E_TYPE:
        case E_TRAIT:
            break;
        }
    }
    iterator_free(entryIter);
}

// insert a member with a given name and pointer to entry, along with info about the entry type
void scope_insert(struct Scope *scope, char *name, void *newEntry, enum SCOPE_MEMBER_TYPE type, enum ACCESS accessibility)
{
    if (scope_contains(scope, name, type))
    {
        InternalError("Error defining symbol [%s] - name already exists!", name);
    }
    struct ScopeMember *wipMember = malloc(sizeof(struct ScopeMember));
    wipMember->name = name;
    wipMember->entry = newEntry;
    wipMember->type = type;
    wipMember->accessibility = accessibility;
    set_insert(scope->entries, wipMember);
}

void scope_remove(struct Scope *scope, char *name, enum SCOPE_MEMBER_TYPE type)
{
    struct ScopeMember dummyMember = {0};
    dummyMember.name = name;
    dummyMember.type = type;
    set_remove(scope->entries, &dummyMember);
}

// create and return a child scope of the scope provided as an argument
struct Scope *scope_create_sub_scope(struct Scope *parent_scope)
{
    if (parent_scope->subScopeCount == U8_MAX)
    {
        InternalError("Too many subscopes of scope %s", parent_scope->name);
    }
    char *helpStr = malloc(2 + strlen(parent_scope->name) + 1);
    sprintf(helpStr, "%02x", parent_scope->subScopeCount);
    char *newScopeName = dictionary_lookup_or_insert(parseDict, helpStr);
    free(helpStr);
    parent_scope->subScopeCount++;

    struct Scope *newScope = scope_new(parent_scope, newScopeName, parent_scope->parentFunction);

    scope_insert(parent_scope, newScopeName, newScope, E_SCOPE, A_PUBLIC);
    return newScope;
}

// create a variable within the given scope
struct VariableEntry *scope_create_variable(struct Scope *scope,
                                            struct Ast *nameTree,
                                            struct Type *type,
                                            bool isGlobal,
                                            enum ACCESS accessibility)
{
    if (scope_contains(scope, nameTree->value, E_VARIABLE) || scope_contains(scope, nameTree->value, E_ARGUMENT))
    {
        log_tree(LOG_FATAL, nameTree, "Redifinition of symbol %s!", nameTree->value);
    }

    return scope_create_variable_by_name(scope, nameTree->value, type, isGlobal, accessibility);
}

// create a variable within the given scope
struct VariableEntry *scope_create_variable_by_name(struct Scope *scope,
                                                    char *name,
                                                    struct Type *type,
                                                    bool isGlobal,
                                                    enum ACCESS accessibility)
{
    if (scope_contains(scope, name, E_VARIABLE) || scope_contains(scope, name, E_ARGUMENT))
    {
        InternalError("Redifinition of symbol %s!", name);
    }

    struct VariableEntry *newVariable = variable_entry_new(name, type, isGlobal, false, accessibility);

    scope_insert(scope, name, newVariable, E_VARIABLE, accessibility);

    return newVariable;
}

// create an argument within the given scope
struct VariableEntry *scope_create_argument(struct Scope *scope,
                                            struct Ast *name,
                                            struct Type *type,
                                            enum ACCESS accessibility)
{

    struct VariableEntry *newArgument = variable_entry_new(name->value, type, false, true, accessibility);

    if (scope_contains(scope, name->value, E_VARIABLE) || scope_contains(scope, name->value, E_ARGUMENT))
    {
        log_tree(LOG_FATAL, name, "Redifinition of symbol %s!", name->value);
    }

    scope_insert(scope, name->value, newArgument, E_ARGUMENT, accessibility);

    return newArgument;
}

// create a new function accessible within the given scope
struct FunctionEntry *scope_create_function(struct Scope *parentScope,
                                            struct Ast *nameTree,
                                            struct Type *returnType,
                                            struct TypeEntry *implementedFor,
                                            enum ACCESS accessibility)
{
    struct FunctionEntry *newFunction = function_entry_new(parentScope, nameTree, implementedFor);
    newFunction->returnType = *returnType;
    scope_insert(parentScope, nameTree->value, newFunction, E_FUNCTION, accessibility);
    return newFunction;
}

struct TypeEntry *scope_create_struct(struct Scope *scope,
                                      char *name)
{
    struct TypeEntry *wipType = type_entry_new_struct(name, scope, G_NONE, NULL);
    scope_insert(scope, name, wipType, E_TYPE, A_PUBLIC);
    return wipType;
}

struct TypeEntry *scope_create_generic_base_struct(struct Scope *scope,
                                                   char *name,
                                                   List *paramNames)
{
    struct TypeEntry *wipType = type_entry_new_struct(name, scope, G_BASE, paramNames);
    scope_insert(scope, name, wipType, E_TYPE, A_PUBLIC);

    return wipType;
}

// TODO: enum_desc_new()
struct TypeEntry *scope_create_enum(struct Scope *scope,
                                    char *name)
{
    struct TypeEntry *wipType = type_entry_new_enum(name, scope, G_NONE, NULL);
    scope_insert(scope, name, wipType, E_TYPE, A_PUBLIC);

    return wipType;
}

struct TypeEntry *scope_create_generic_base_enum(struct Scope *scope,
                                                 char *name,
                                                 List *paramNames)
{
    struct TypeEntry *wipType = type_entry_new_enum(name, scope, G_BASE, paramNames);
    scope_insert(scope, name, wipType, E_TYPE, A_PUBLIC);

    return wipType;
}

struct TraitEntry *scope_create_trait(struct Scope *scope,
                                      char *name)
{
    struct TraitEntry *newTrait = trait_new(name, scope);
    scope_insert(scope, name, newTrait, E_TRAIT, A_PUBLIC);
    return newTrait;
}

// Scope lookup functions

bool scope_contains(struct Scope *scope, char *name, enum SCOPE_MEMBER_TYPE type)
{
    struct ScopeMember dummyMember = {0};
    dummyMember.name = name;
    dummyMember.type = type;
    return (set_find(scope->entries, &dummyMember) != NULL);
}

struct ScopeMember *scope_lookup_no_parent(struct Scope *scope, char *name, enum SCOPE_MEMBER_TYPE type)
{
    struct ScopeMember dummyMember = {0};
    dummyMember.name = name;
    dummyMember.type = type;
    return set_find(scope->entries, &dummyMember);
}

// if a member with the given name exists in this scope or any of its parents, return it
// also looks up entries from deeper scopes, but only as their mangled names specify
struct ScopeMember *scope_lookup(struct Scope *scope, char *name, enum SCOPE_MEMBER_TYPE type)
{
    while (scope != NULL)
    {
        struct ScopeMember *foundThisScope = scope_lookup_no_parent(scope, name, type);
        if (foundThisScope != NULL)
        {
            return foundThisScope;
        }
        scope = scope->parentScope;
    }
    return NULL;
}

void scope_add_basic_block(struct Scope *scope, struct BasicBlock *block)
{
    const u8 BASIC_BLOCK_NAME_STR_SIZE = 10; // TODO: manage this better
    char *blockName = malloc(BASIC_BLOCK_NAME_STR_SIZE);
    sprintf(blockName, "Block%zu", block->labelNum);
    char *dictBlockName = dictionary_lookup_or_insert(parseDict, blockName);
    free(blockName);
    scope_insert(scope, dictBlockName, block, E_BASICBLOCK, A_PUBLIC);

    if (scope->parentFunction != NULL)
    {
        list_append(scope->parentFunction->BasicBlockList, block);
    }
}

struct VariableEntry *scope_lookup_var_by_string(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUpVar = scope_lookup(scope, name, E_VARIABLE);
    struct ScopeMember *lookedUpArg = scope_lookup(scope, name, E_ARGUMENT);
    if ((lookedUpVar == NULL) && (lookedUpArg == NULL))
    {
        return NULL;
    }

    struct ScopeMember *lookedUp = (lookedUpVar != NULL) ? lookedUpVar : lookedUpArg;

    switch (lookedUp->type)
    {
    case E_ARGUMENT:
    case E_VARIABLE:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up variable [%s]!", name);
    }

    return NULL;
}

struct VariableEntry *scope_lookup_var(struct Scope *scope, struct Ast *nameTree)
{
    struct VariableEntry *lookedUp = scope_lookup_var_by_string(scope, nameTree->value);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared variable '%s'", nameTree->value);
    }

    return lookedUp;
}

struct FunctionEntry *lookup_fun_by_string(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name, E_FUNCTION);
    if (lookedUp == NULL)
    {
        InternalError("Lookup of undeclared function '%s'", name);
    }

    switch (lookedUp->type)
    {
    case E_FUNCTION:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up function!");
    }
}

struct FunctionEntry *scope_lookup_fun(struct Scope *scope, struct Ast *nameTree)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, nameTree->value, E_FUNCTION);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared function '%s'", nameTree->value);
    }
    switch (lookedUp->type)
    {
    case E_FUNCTION:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up function!");
    }
}

struct StructDesc *scope_lookup_struct(struct Scope *scope,
                                       struct Ast *nameTree)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, nameTree->value, E_TYPE);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared struct '%s'", nameTree->value);
    }

    switch (lookedUp->type)
    {
    case E_TYPE:
    {
        struct TypeEntry *lookedUpType = lookedUp->entry;
        switch (lookedUpType->permutation)
        {
        case TP_PRIMITIVE:
            log_tree(LOG_FATAL, nameTree, "%s is a primitive type, not a struct!", nameTree->value);
            break;

        case TP_STRUCT:
            return lookedUpType->data.asStruct;

        case TP_ENUM:
            log_tree(LOG_FATAL, nameTree, "%s is an enum type, not a struct!", nameTree->value);
            break;
        }
    }
    break;

    default:
        log_tree(LOG_FATAL, nameTree, "%s is not a struct!", nameTree->value);
    }

    return NULL;
}

struct StructDesc *scope_lookup_struct_by_type(struct Scope *scope,
                                               struct Type *type)
{
    if (type->basicType == VT_SELF)
    {
        if ((scope->parentFunction == NULL) || (scope->parentFunction->implementedFor == NULL))
        {
            InternalError("Use of 'Self' outside of impl context!");
        }

        if (scope->parentFunction->implementedFor->permutation != TP_STRUCT)
        {
            InternalError("Non-struct type %s of current scope->implementedFor seen in scope_lookup_struct_by_type!", type_entry_name(scope->parentFunction->implementedFor));
        }

        return scope->parentFunction->implementedFor->data.asStruct;
    }

    if (type->basicType != VT_STRUCT || type->nonArray.complexType.name == NULL)
    {
        InternalError("Non-struct type or struct type with null name passed to lookupStructByType!");
    }

    struct TypeEntry *lookedUpType = scope_lookup_type(scope, type);
    if (lookedUpType == NULL)
    {
        InternalError("Use of undeclared struct '%s'", type->nonArray.complexType.name);
    }

    if (lookedUpType->permutation != TP_STRUCT)
    {
        InternalError("Use of non-struct type '%s'", type->nonArray.complexType.name);
    }

    struct StructDesc *lookedUpStruct = lookedUpType->data.asStruct;

    switch (lookedUpType->genericType)
    {
    case G_NONE:
        break;

    case G_BASE:
    {
        if (type->nonArray.complexType.genericParams != NULL)
        {
            lookedUpType = type_entry_get_or_create_generic_instantiation(lookedUpType, type->nonArray.complexType.genericParams);
            lookedUpStruct = lookedUpType->data.asStruct;
        }
    }
    break;

    case G_INSTANCE:
        if (type->nonArray.complexType.genericParams != NULL)
        {

            Iterator *expectedIter = list_begin(lookedUpType->generic.instance.parameters);
            Iterator *actualIter = list_begin(type->nonArray.complexType.genericParams);
            bool paramMismatch = false;
            while (iterator_gettable(expectedIter) && iterator_gettable(actualIter))
            {
                struct Type *expectedParam = iterator_get(expectedIter);
                struct Type *actualParam = iterator_get(actualIter);
                if (type_compare(expectedParam, actualParam))
                {
                    paramMismatch = true;
                }

                iterator_next(expectedIter);
                iterator_next(actualIter);
            }

            if (paramMismatch || iterator_gettable(expectedIter) || iterator_gettable(actualIter))
            {
                InternalError("Mismatch in generic params for type %s - saw %s, expected %s", type_get_name(type),
                              sprint_generic_params(lookedUpType->generic.instance.parameters),
                              sprint_generic_params(type->nonArray.complexType.genericParams));
            }

            iterator_free(expectedIter);
            iterator_free(actualIter);
        }
    }

    return lookedUpStruct;
}

struct StructDesc *scope_lookup_struct_by_type_or_pointer(struct Scope *scope, struct Type *type)
{
    struct Type typeToLookup = *type;
    typeToLookup.pointerLevel = 0;
    return scope_lookup_struct_by_type(scope, &typeToLookup);
}

struct StructDesc *scope_lookup_struct_by_name(struct Scope *scope,
                                               char *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name, E_TYPE);
    if (lookedUp == NULL)
    {
        InternalError("Use of undeclared struct '%s'", name);
    }

    struct TypeEntry *lookedUpType = lookedUp->entry;

    switch (lookedUpType->permutation)
    {
    case TP_PRIMITIVE:
        InternalError("%s names a primitive type, not a struct", name);
        break;

    case TP_STRUCT:
        return lookedUpType->data.asStruct;

    case TP_ENUM:
        InternalError("%s names an enum type, not a struct", name);
        break;
    }

    return NULL;
}

struct EnumDesc *scope_lookup_enum(struct Scope *scope,
                                   struct Ast *nameTree)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, nameTree->value, E_TYPE);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared enum '%s'", nameTree->value);
    }

    struct TypeEntry *lookedUpType = lookedUp->entry;

    switch (lookedUpType->permutation)
    {
    case TP_PRIMITIVE:
        log_tree(LOG_FATAL, nameTree, "%s names a primitive type, not an enum", nameTree->value);
        break;

    case TP_STRUCT:
        log_tree(LOG_FATAL, nameTree, "%s names a struct type, not an enum", nameTree->value);
        break;

    case TP_ENUM:
        return lookedUpType->data.asEnum;
    }

    return NULL;
}

struct EnumDesc *scope_lookup_enum_by_type(struct Scope *scope,
                                           struct Type *type)
{
    if (type->basicType != VT_ENUM || type->nonArray.complexType.name == NULL)
    {
        InternalError("Non-enum type or enum type with null name passed to lookupEnumByType!");
    }

    struct ScopeMember *lookedUp = scope_lookup(scope, type->nonArray.complexType.name, E_TYPE);
    if (lookedUp == NULL)
    {
        log(LOG_FATAL, "Use of undeclared enum '%s'", type->nonArray.complexType.name);
    }

    struct TypeEntry *lookedUpType = lookedUp->entry;

    switch (lookedUpType->permutation)
    {
    case TP_PRIMITIVE:
        InternalError("%s names a primitive type, not a struct", type->nonArray.complexType.name);

    case TP_STRUCT:
        InternalError("%s names a struct type, not a struct", type->nonArray.complexType.name);

    case TP_ENUM:
        return lookedUpType->data.asEnum;
    }

    return NULL;
}

struct EnumDesc *scope_lookup_enum_by_member_name(struct Scope *scope,
                                                  char *name)
{
    struct EnumMember dummyMember = {0};
    dummyMember.name = name;

    while (scope != NULL)
    {
        Iterator *memberIterator = NULL;
        for (memberIterator = set_begin(scope->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
        {
            struct ScopeMember *member = iterator_get(memberIterator);
            if (member->type == E_TYPE)
            {
                struct TypeEntry *memberType = member->entry;
                if (memberType->permutation == TP_ENUM)
                {
                    struct EnumDesc *scannedEnum = memberType->data.asEnum;
                    if (set_find(scannedEnum->members, &dummyMember) != NULL)
                    {
                        iterator_free(memberIterator);
                        return scannedEnum;
                    }
                }
            }
        }
        iterator_free(memberIterator);
        scope = scope->parentScope;
    }

    return NULL;
}

struct BasicBlock *scope_lookup_block_by_number(struct Scope *scope, size_t label)
{
    char blockName[32];
    sprintf(blockName, "Block%zu", label);
    struct ScopeMember *blockMember = scope_lookup(scope, blockName, E_BASICBLOCK);
    if (blockMember == NULL)
    {
        return NULL;
    }
    return blockMember->entry;
}

// TODO: better param names than toClone and cloneTo. seriously.
struct FunctionEntry *function_entry_clone(struct FunctionEntry *toClone, struct Scope *cloneTo, struct TypeEntry *newImplementedFor)
{
    log(LOG_DEBUG, "function_entry_clone: %s", toClone->name);
    struct FunctionEntry *cloned = function_entry_new(cloneTo, &toClone->correspondingTree, newImplementedFor);
    cloned->returnType = type_duplicate_non_pointer(&toClone->returnType);
    cloned->callsOtherFunction = toClone->callsOtherFunction;
    cloned->isAsmFun = toClone->isAsmFun;
    cloned->isDefined = toClone->isDefined;
    cloned->isMethod = toClone->isMethod;

    scope_clone_to(cloned->mainScope, toClone->mainScope, newImplementedFor);

    log(LOG_DEBUG, "initialize arguments list for clone of function %s", toClone->name);
    Iterator *argIter = NULL;
    for (argIter = deque_front(toClone->arguments); iterator_gettable(argIter); iterator_next(argIter))
    {
        struct VariableEntry *oldArg = iterator_get(argIter);
        struct VariableEntry *newArg = scope_lookup_var_by_string(cloned->mainScope, oldArg->name);
        if (newArg == NULL)
        {
            InternalError("Couldn't find expected argument %s when cloning function %s", oldArg->name, toClone->name);
        }
        log(LOG_DEBUG, "%s", newArg->name);
        deque_push_back(cloned->arguments, newArg);
    }
    iterator_free(argIter);

    log(LOG_DEBUG, "initialize basic block list for clone of function %s", toClone->name);
    Iterator *blockIter = NULL;
    for (blockIter = list_begin(toClone->BasicBlockList); iterator_gettable(blockIter); iterator_next(blockIter))
    {
        struct BasicBlock *oldBlock = iterator_get(blockIter);
        struct BasicBlock *newBlock = scope_lookup_block_by_number(cloned->mainScope, oldBlock->labelNum);
        if (newBlock == NULL)
        {
            InternalError("Couldn't find expected basic block %zu when cloning function %s", oldBlock->labelNum, toClone->name);
        }
        log(LOG_DEBUG, "%zu", newBlock->labelNum);

        list_append(cloned->BasicBlockList, newBlock);
    }
    iterator_free(blockIter);

    return cloned;
}

struct BasicBlock *basic_block_clone(struct BasicBlock *toClone, struct Scope *clonedTo)
{
    struct BasicBlock *clone = basic_block_new(toClone->labelNum);

    Iterator *tacRunner = NULL;
    for (tacRunner = list_begin(toClone->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
    {
        struct TACLine *lineToClone = iterator_get(tacRunner);

        // TODO: tac_line_duplicate
        struct TACLine *clonedLine = new_tac_line(lineToClone->operation, &lineToClone->correspondingTree);
        memcpy(clonedLine, lineToClone, sizeof(struct TACLine));
        clonedLine->allocFile = __FILE__;
        clonedLine->allocLine = __LINE__;

        struct OperandUsages operandUsages = get_operand_usages(clonedLine);
        while (operandUsages.reads->size > 0)
        {
            struct TACOperand *readOperand = deque_pop_front(operandUsages.reads);
            switch (readOperand->permutation)
            {
            case VP_STANDARD:
            case VP_TEMP:
                readOperand->name.variable = scope_lookup_var_by_string(clonedTo, readOperand->name.variable->name);
                break;

            default:
                break;
            }
        }

        while (operandUsages.writes->size > 0)
        {
            struct TACOperand *writtenOperand = deque_pop_front(operandUsages.writes);
            switch (writtenOperand->permutation)
            {
            case VP_STANDARD:
            case VP_TEMP:
                writtenOperand->name.variable = scope_lookup_var_by_string(clonedTo, writtenOperand->name.variable->name);
                break;

            default:
                break;
            }
        }

        deque_free(operandUsages.reads);
        deque_free(operandUsages.writes);

        if (clonedLine->operation == TT_ASSOCIATED_CALL)
        {
            clonedLine->operands.associatedCall.associatedWith = clonedTo->parentFunction->implementedFor->type;
        }

        size_t tacIndex = lineToClone->index;
        basic_block_append(clone, clonedLine, &tacIndex);
    }
    iterator_free(tacRunner);

    return clone;
}

void scope_clone_to(struct Scope *clonedTo, struct Scope *toClone, struct TypeEntry *newImplementedFor)
{
    log(LOG_DEBUG, "scope_clone_to %s<-%s", clonedTo->name, toClone->name);
    Iterator *memberIterator = NULL;
    for (memberIterator = set_begin(toClone->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
    {
        struct ScopeMember *memberToClone = iterator_get(memberIterator);
        void *entry = NULL;
        switch (memberToClone->type)
        {
        case E_VARIABLE:
        {
            struct VariableEntry *variableToClone = memberToClone->entry;
            struct Type dupType = type_duplicate_non_pointer(&variableToClone->type);
            entry = variable_entry_new(variableToClone->name, &dupType, variableToClone->isGlobal, false, memberToClone->accessibility);
        }
        break;

        case E_ARGUMENT:
        {
            struct VariableEntry *argumentToClone = memberToClone->entry;
            struct Type dupType = type_duplicate_non_pointer(&argumentToClone->type);
            entry = variable_entry_new(argumentToClone->name, &dupType, argumentToClone->isGlobal, true, memberToClone->accessibility);
        }
        break;

        case E_SCOPE:
        {
            struct Scope *clonedScope = memberToClone->entry;
            struct Scope *clonedSubScope = scope_new(clonedTo, clonedScope->name, clonedTo->parentFunction);
            entry = clonedSubScope;
            scope_clone_to(entry, clonedScope, newImplementedFor);
        }
        break;

        case E_BASICBLOCK:
        case E_FUNCTION:
        case E_TYPE:
        case E_TRAIT:
            break;
        }

        if (entry != NULL)
        {
            scope_insert(clonedTo, memberToClone->name, entry, memberToClone->type, memberToClone->accessibility);
        }
    }
    iterator_free(memberIterator);

    for (memberIterator = set_begin(toClone->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
    {
        struct ScopeMember *memberToClone = iterator_get(memberIterator);
        void *entry = NULL;
        switch (memberToClone->type)
        {
        case E_FUNCTION:
        {
            struct FunctionEntry *clonedFunction = memberToClone->entry;
            entry = function_entry_clone(clonedFunction, clonedTo, newImplementedFor);
        }
        break;

        case E_TYPE:
            InternalError("scope_clone on types not yet implemented!");

        case E_BASICBLOCK:
        {
            struct BasicBlock *clonedBlock = memberToClone->entry;
            entry = basic_block_clone(clonedBlock, clonedTo);
        }
        break;

        case E_VARIABLE:
        case E_ARGUMENT:
        case E_SCOPE:
        case E_TRAIT:
            break;
        }

        if (entry != NULL)
        {
            scope_insert(clonedTo, memberToClone->name, entry, memberToClone->type, memberToClone->accessibility);
        }
    }
    iterator_free(memberIterator);
    // case E_FUNCTION:
    // case E_STRUCT:
    // case E_ENUM:

    // {
    //
    // }
    // break;
}

void basic_block_resolve_generics(struct BasicBlock *block, HashTable *paramsMap, char *resolvedStructName, List *resolvedParams)
{
    Iterator *tacRunner = NULL;
    for (tacRunner = list_begin(block->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
    {
        struct TACLine *resolvedLine = iterator_get(tacRunner);
        struct OperandUsages operandUsages = get_operand_usages(resolvedLine);
        while (operandUsages.reads->size > 0)
        {
            struct TACOperand *readOperand = deque_pop_front(operandUsages.reads);
            type_try_resolve_generic(&readOperand->castAsType, paramsMap, resolvedStructName, resolvedParams);
        }

        while (operandUsages.writes->size > 0)
        {
            struct TACOperand *writtenOperand = deque_pop_front(operandUsages.writes);
            type_try_resolve_generic(&writtenOperand->castAsType, paramsMap, resolvedStructName, resolvedParams);
        }

        deque_free(operandUsages.reads);
        deque_free(operandUsages.writes);
    }
    iterator_free(tacRunner);
}

void scope_resolve_generics(struct Scope *scope, HashTable *paramsMap, char *resolvedStructName, List *resolvedParams)
{
    Iterator *memberIterator = NULL;
    for (memberIterator = set_begin(scope->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
    {
        struct ScopeMember *memberToResolve = iterator_get(memberIterator);
        switch (memberToResolve->type)
        {
        case E_VARIABLE:
        case E_ARGUMENT:
        {
            struct VariableEntry *resolved = memberToResolve->entry;
            type_try_resolve_generic(&resolved->type, paramsMap, resolvedStructName, resolvedParams);
        }
        break;

        case E_FUNCTION:
        {
            struct FunctionEntry *resolved = memberToResolve->entry;
            type_try_resolve_generic(&resolved->returnType, paramsMap, resolvedStructName, resolvedParams);
            scope_resolve_generics(resolved->mainScope, paramsMap, resolvedStructName, resolvedParams);
        }
        break;

        case E_TYPE:
        {
            InternalError("Recursive generic resolution for sub-types not implemented yet!");
        }
        break;

        case E_SCOPE:
        {
            scope_resolve_generics(memberToResolve->entry, paramsMap, resolvedStructName, resolvedParams);
        }
        break;

        case E_BASICBLOCK:
        {
            struct BasicBlock *resolvedBlock = memberToResolve->entry;
            basic_block_resolve_generics(resolvedBlock, paramsMap, resolvedStructName, resolvedParams);
        }
        break;

        case E_TRAIT:
        {
            InternalError("Recursive generic resolution for sub-traits not implemented yet!");
        }
        break;
        }
    }
    iterator_free(memberIterator);
}

struct TypeEntry *scope_lookup_type_by_name(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name, E_TYPE);
    if (lookedUp == NULL)
    {
        InternalError("Use of undeclared type '%s'", name);
    }

    switch (lookedUp->type)
    {
    case E_TYPE:
        return lookedUp->entry;

    default:
        InternalError("scope_lookup_type_by_name for %s lookup got a non-type ScopeMember!", name);
    }
}

struct TypeEntry *scope_lookup_struct_by_name_tree(struct Scope *scope, struct Ast *nameTree)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, nameTree->value, E_TYPE);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared type '%s'", nameTree->value);
    }

    struct TypeEntry *lookedUpType = NULL;

    switch (lookedUp->type)
    {
    case E_TYPE:
        lookedUpType = lookedUp->entry;
        break;

    default:
        log_tree(LOG_FATAL, nameTree, "scope_lookup_type_by_name for %s lookup got a non-type ScopeMember!", nameTree->value);
    }

    return lookedUpType;
}

struct TypeEntry *scope_lookup_type(struct Scope *scope, struct Type *type)
{
    if ((type->basicType == VT_NULL) || (type->basicType == VT_GENERIC_PARAM) || type->pointerLevel > 0)
    {
        InternalError("Invalid type %s passed to scope_lookup_type", type_get_name(type));
    }

    struct TypeEntry *lookedUp = NULL;

    switch (type->basicType)
    {
    case VT_NULL:
        InternalError("saw VT_NULL in scope_lookup_type!");
        break;

    case VT_GENERIC_PARAM:
        InternalError("saw VT_GENERIC_PARAM in scope_lookup_type!");
        break;

    case VT_U8:
    case VT_U16:
    case VT_U32:
    case VT_U64:
    case VT_ANY:
    case VT_ARRAY:
        InternalError("scope_lookup_type for primitives not yet implemented!");
        return NULL;

    case VT_STRUCT:
    case VT_ENUM:
    {
        struct ScopeMember *lookedUpEntry = scope_lookup(scope, type->nonArray.complexType.name, E_TYPE);
        if (lookedUpEntry == NULL)
        {
            InternalError("Use of undeclared type '%s'", type->nonArray.complexType.name);
        }
        lookedUp = lookedUpEntry->entry;

        if ((type->nonArray.complexType.genericParams != NULL))
        {
            switch (lookedUp->genericType)
            {
            case G_BASE:
            {
                lookedUp = type_entry_get_or_create_generic_instantiation(lookedUp, type->nonArray.complexType.genericParams);
            }
            break;

            case G_INSTANCE:
                InternalError("Complex type with generic params %s return non-generic type", sprint_generic_params(type->nonArray.complexType.genericParams));
                break;

            case G_NONE:
                InternalError("Complex type with generic params %s return non-generic type", sprint_generic_params(type->nonArray.complexType.genericParams));
                break;
            }
        }
    }
    break;

    case VT_SELF:
        if ((scope->parentFunction == NULL) || (scope->parentFunction->implementedFor == NULL))
        {
            InternalError("Use of 'Self' outside of impl context!");
        }
        lookedUp = scope->parentFunction->implementedFor;
    }

    return lookedUp;
}

struct TypeEntry *scope_lookup_type_remove_pointer(struct Scope *scope, struct Type *type)
{
    struct Type typeToLookup = *type;
    typeToLookup.pointerLevel = 0;
    return scope_lookup_type(scope, &typeToLookup);
}

struct TraitEntry *scope_lookup_trait(struct Scope *scope, struct Ast *nameTree)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, nameTree->value, E_TRAIT);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, nameTree, "Use of undeclared trait '%s'", nameTree->value);
    }

    switch (lookedUp->type)
    {
    case E_TRAIT:
        return lookedUp->entry;

    default:
        log_tree(LOG_FATAL, nameTree, "scope_lookup_trait for %s lookup got a non-trait ScopeMember!", nameTree->value);
    }

    return NULL;
}
