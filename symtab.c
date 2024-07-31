#include <errno.h>
#include <sys/stat.h>

#include "codegen_generic.h"
#include "symtab.h"
#include "symtab_scope.h"
#include <mbcl/stack.h>

#include "log.h"

extern struct Dictionary *parseDict;

void scope_print_member(struct ScopeMember *toPrint, bool printTac, size_t depth, FILE *outFile);

struct SymbolTable *symbol_table_new(char *name)
{
    struct SymbolTable *wip = malloc(sizeof(struct SymbolTable));
    wip->name = name;
    wip->globalScope = scope_new(NULL, "Global", NULL, NULL);
    struct BasicBlock *globalBlock = basic_block_new(0);

    // manually insert a basic block for global code so we can give it the custom name of "globalblock"
    scope_insert(wip->globalScope, "globalblock", globalBlock, E_BASICBLOCK, A_PUBLIC);

    return wip;
}

void symbol_table_print(struct SymbolTable *table, FILE *outFile, bool printTac)
{
    printf("~~~~~~~~~~~~~\n");
    printf("Symbol Table For %s:\n\n", table->name);
    scope_print(table->globalScope, outFile, 0, printTac);
    printf("~~~~~~~~~~~~~\n\n");
}

void scope_print_cfgs(struct Scope *scope, char *outDir)
{
    // check if the directory exists
    struct stat st = {0};
    if (stat(outDir, &st) == -1)
    {
        if (mkdir(outDir, 0777) == -1)
        {
            InternalError("Couldn't create cfg directory %s: %s", outDir, strerror(errno));
        }
    }
    Iterator *memberIterator = NULL;
    for (memberIterator = set_begin(scope->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
    {
        struct ScopeMember *thisMember = iterator_get(memberIterator);

        switch (thisMember->type)
        {
        case E_FUNCTION:
        {
            struct FunctionEntry *thisFunction = thisMember->entry;
            char *cfgFileName = malloc(strlen(outDir) + strlen(thisFunction->name) + 7);
            sprintf(cfgFileName, "%s/%s.dot", outDir, thisFunction->name);
            FILE *cfgFile = fopen(cfgFileName, "w");
            if (cfgFile == NULL)
            {
                InternalError("Couldn't open cfg file %s: %s", cfgFileName, strerror(errno));
            }
            function_entry_print_cfg(thisFunction, cfgFile);
            fclose(cfgFile);
            free(cfgFileName);
        }
        break;

        case E_SCOPE:
        {
            struct Scope *thisScope = thisMember->entry;
            scope_print_cfgs(thisScope, outDir);
        }
        break;

        case E_STRUCT:
        {
            struct StructEntry *thisStruct = thisMember->entry;
            char *structCfgDirName = malloc(strlen(outDir) + strlen(thisStruct->name) + 3);

            sprintf(structCfgDirName, "%s/%s", outDir, thisStruct->name);
            scope_print_cfgs(thisStruct->members, structCfgDirName);
            free(structCfgDirName);
        }
        break;

        default:
            break;
        }
    }
    iterator_free(memberIterator);
}

void symbol_table_print_cfgs(struct SymbolTable *table, char *outDir)
{
    scope_print_cfgs(table->globalScope, outDir);
}

char *symbol_table_mangle_name(struct Scope *scope, struct Dictionary *dict, char *toMangle)
{
    char *scopeName = scope->name;

    char *mangledName = malloc(strlen(toMangle) + strlen(scopeName) + 2);
    sprintf(mangledName, "%s.%s", scopeName, toMangle);
    char *newName = dictionary_lookup_or_insert(dict, mangledName);
    free(mangledName);
    return newName;
}

void scope_lift_from_sub_scopes(struct Scope *scope, struct Dictionary *dict, size_t depth)
{
    log(LOG_DEBUG, "%*s%zu lifting subscopes/functions/structs from within scope %s", depth * 4, "", depth, scope->name);

    Set *moveToThisScope = set_new(NULL, scope->entries->compareData);
    Set *deletedSubScopes = set_new(NULL, scope->entries->compareData);
    Iterator *memberIterator = NULL;
    for (memberIterator = set_begin(scope->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
    {
        struct ScopeMember *thisMember = iterator_get(memberIterator);
        switch (thisMember->type)
        {
        case E_SCOPE: // recurse to subscopes
        {
            moveToThisScope = set_union_destructive(moveToThisScope, symbol_table_collapse_scopes_rec(thisMember->entry, dict, depth + 1));
            set_insert(deletedSubScopes, thisMember);
        }
        break;

        case E_FUNCTION: // recurse to functions
        {
            if (depth > 0)
            {
                InternalError("Saw function at depth > 0 when collapsing scopes!");
            }
            struct FunctionEntry *thisFunction = thisMember->entry;
            moveToThisScope = set_union_destructive(moveToThisScope, symbol_table_collapse_scopes_rec(thisFunction->mainScope, dict, 0));
        }
        break;

        case E_STRUCT:
        {
            struct StructEntry *recursedStruct = thisMember->entry;
            moveToThisScope = set_union_destructive(moveToThisScope, symbol_table_collapse_scopes_rec(recursedStruct->members, dict, 0));
        }
        break;

        // skip everything else
        case E_VARIABLE:
        case E_ARGUMENT:
        case E_BASICBLOCK:
        case E_ENUM:
            break;
        }
    }
    iterator_free(memberIterator);

    Iterator *moveHereIterator = NULL;
    for (moveHereIterator = set_begin(moveToThisScope); iterator_gettable(moveHereIterator); iterator_next(moveHereIterator))
    {
        struct ScopeMember *toMoveHere = iterator_get(moveHereIterator);
        set_insert(scope->entries, toMoveHere);
    }
    iterator_free(moveHereIterator);
    set_free(moveToThisScope);

    Iterator *removeFromHereIterator = NULL;
    for (removeFromHereIterator = set_begin(deletedSubScopes); iterator_gettable(removeFromHereIterator); iterator_next(removeFromHereIterator))
    {
        struct ScopeMember *removedMember = iterator_get(removeFromHereIterator);
        log(LOG_DEBUG, "%*s%zu Subscope %s is no longer needed - delete it", depth * 4, "", depth, removedMember->name);
        set_remove(scope->entries, removedMember);
    }
    iterator_free(removeFromHereIterator);
    set_free(deletedSubScopes);

    log(LOG_DEBUG, "%*s%zu DONE lifting subscopes/functions/structs from within scope %s", depth * 4, "", depth, scope->name);
}

Set *symbol_table_collapse_scopes_rec(struct Scope *scope, struct Dictionary *dict, size_t depth)
{
    log(LOG_DEBUG, "%*s%zu Collapsing scopes recursively for scope %s", depth * 4, "", depth, scope->name);

    scope_lift_from_sub_scopes(scope, dict, depth);

    Iterator *memberIterator = NULL;
    Stack *moveOutOfThisScope = stack_new(NULL);
    // perform all recursive operations first
    for (memberIterator = set_begin(scope->entries); iterator_gettable(memberIterator); iterator_next(memberIterator))
    {
        struct ScopeMember *thisMember = iterator_get(memberIterator);
        switch (thisMember->type)
        {
        case E_SCOPE:
        case E_FUNCTION:
            break;

        case E_BASICBLOCK:
        {
            if ((depth > 0) && (scope->parentScope != NULL))
            {
                stack_push(moveOutOfThisScope, thisMember);
            }
        }
        break;

        case E_VARIABLE:
        case E_ARGUMENT:
        {
            struct VariableEntry *variableToMove = thisMember->entry;

            if (((depth > 0) || variableToMove->isGlobal) && scope->parentScope != NULL)
            {
                stack_push(moveOutOfThisScope, thisMember);
            }
        }
        break;

        case E_STRUCT:
        case E_ENUM:
            break;
        }
    }

    log(LOG_DEBUG, "%*s%zu Moving %zu elements out of scope", depth * 4, "", depth, moveOutOfThisScope->size);
    iterator_free(memberIterator);
    MBCL_DATA_FREE_FUNCTION oldFree = scope->entries->freeData;
    scope->entries->freeData = NULL;

    Set *renamedAndMoved = set_new(NULL, scope->entries->compareData);

    while (moveOutOfThisScope->size > 0)
    {
        struct ScopeMember *moved = stack_pop(moveOutOfThisScope);
        log(LOG_DEBUG, "%*s%zu moving %s", depth * 4, "", depth, moved->name);
        set_remove(scope->entries, moved);
        // TODO: actually mangle names
        // mangle all non-global names (want to mangle everything except for string literal names)
        if ((moved->type == E_VARIABLE) || (moved->type == E_ARGUMENT))
        {
            struct VariableEntry *variableToMove = moved->entry;
            // we will only ever do anything if we are depth >0 or need to kick a global variable up a scope
            if ((depth > 0) && (!variableToMove->isGlobal))
            {
                moved->name = symbol_table_mangle_name(scope, dict, moved->name);
                variableToMove->name = moved->name;
            }
        }
        set_insert(renamedAndMoved, moved);
    }
    scope->entries->freeData = oldFree;
    stack_free(moveOutOfThisScope);

    log(LOG_DEBUG, "%*s%zu DONE collapsing scopes recursively for scope %s", depth * 4, "", depth, scope->name);
    return renamedAndMoved;
}

void symbol_table_collapse_scopes(struct SymbolTable *table, struct Dictionary *dict)
{
    Set *topLevelMoved = symbol_table_collapse_scopes_rec(table->globalScope, parseDict, 0);
    if (topLevelMoved->size > 0)
    {
        InternalError("Saw %zu elements to be moved up from global scope", topLevelMoved->size);
    }
    set_free(topLevelMoved);
}

void symbol_table_free(struct SymbolTable *table)
{
    scope_free(table->globalScope);
    free(table);
}

void variable_entry_print(struct VariableEntry *variable, FILE *outFile, size_t depth)
{
    char *typeName = type_get_name(&variable->type);
    fprintf(outFile, "%s %s\n", typeName, variable->name);
    free(typeName);
}

void scope_print_member(struct ScopeMember *toPrint, bool printTac, size_t depth, FILE *outFile)
{
    for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
    {
        fprintf(outFile, "\t");
    }

    switch (toPrint->type)
    {
    case E_ARGUMENT:
    {
        struct VariableEntry *theArgument = toPrint->entry;
        fprintf(outFile, "> Argument: ");
        variable_entry_print(theArgument, outFile, depth);
        for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
        {
            fprintf(outFile, "\t");
        }
        fprintf(outFile, "  - Stack offset: %zd\n", theArgument->stackOffset);
    }
    break;

    case E_VARIABLE:
    {
        struct VariableEntry *theVariable = toPrint->entry;
        fprintf(outFile, "> ");
        variable_entry_print(theVariable, outFile, depth);
    }
    break;

    case E_STRUCT:
    {
        struct StructEntry *theStruct = toPrint->entry;
        fprintf(outFile, "> Struct %s:\n", toPrint->name);
        for (size_t j = 0; j < depth; j++)
        {
            fprintf(outFile, "\t");
        }
        fprintf(outFile, "  - Size: %zu bytes\n", theStruct->totalSize);
        scope_print(theStruct->members, outFile, depth + 1, printTac);
    }
    break;

    case E_ENUM:
    {
        struct EnumEntry *theEnum = toPrint->entry;
        fprintf(outFile, "> Enum %s - data union size of %zu bytes:\n", toPrint->name, theEnum->unionSize);
        Iterator *enumMemberIterator = NULL;
        for (enumMemberIterator = set_begin(theEnum->members); iterator_gettable(enumMemberIterator); iterator_next(enumMemberIterator))
        {
            struct EnumMember *member = iterator_get(enumMemberIterator);
            for (size_t j = 0; j < depth; j++)
            {
                fprintf(outFile, "\t");
            }
            char *memberTypeName = type_get_name(&member->type);
            fprintf(outFile, "%zu:%s (%s)\n", member->numerical, member->name, memberTypeName);
            free(memberTypeName);
        }
        iterator_free(enumMemberIterator);
    }
    break;

    case E_FUNCTION:
    {
        struct FunctionEntry *theFunction = toPrint->entry;
        char *returnTypeName = type_get_name(&theFunction->returnType);
        if (theFunction->methodOf != NULL)
        {
            fprintf(outFile, "> Method %s.", theFunction->methodOf->name);
        }
        else
        {
            fprintf(outFile, "> Function ");
        }
        fprintf(outFile, "%s (returns %s) (defined: %d)\n", toPrint->name, returnTypeName, theFunction->isDefined);
        free(returnTypeName);
        scope_print(theFunction->mainScope, outFile, depth + 1, printTac);
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

void scope_add_basic_block(struct Scope *scope, struct BasicBlock *block)
{
    const u8 BASIC_BLOCK_NAME_STR_SIZE = 10; // TODO: manage this better
    char *blockName = malloc(BASIC_BLOCK_NAME_STR_SIZE);
    sprintf(blockName, "Block%zu", block->labelNum);
    scope_insert(scope, dictionary_lookup_or_insert(parseDict, blockName), block, E_BASICBLOCK, A_PUBLIC);
    free(blockName);

    if (scope->parentFunction != NULL)
    {
        list_append(scope->parentFunction->BasicBlockList, block);
    }
}

/*
 * AST walk and symbol table generation functions
 */

// scrape down a chain of adjacent sibling star tokens, expecting something at the bottom
size_t scrape_pointers(struct Ast *pointerAst, struct Ast **resultDestination)
{
    size_t dereferenceDepth = 0;
    pointerAst = pointerAst->sibling;

    while ((pointerAst != NULL) && (pointerAst->type == T_DEREFERENCE))
    {
        dereferenceDepth++;
        pointerAst = pointerAst->sibling;
    }

    *resultDestination = pointerAst;
    return dereferenceDepth;
}
