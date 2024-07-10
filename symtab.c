#include "symtab.h"
#include "codegen_generic.h"
#include "symtab_scope.h"

#include "log.h"

extern struct Dictionary *parseDict;

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

void scope_decay_arrays(struct Scope *scope)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case E_SCOPE:
            scope_decay_arrays(thisMember->entry);
            break;

        case E_FUNCTION:
        {
            struct FunctionEntry *decayFunction = thisMember->entry;
            scope_decay_arrays(decayFunction->mainScope);
        }
        break;

        case E_BASICBLOCK:
        {
            struct BasicBlock *decayBlock = thisMember->entry;
            for (struct LinkedListNode *tacRunner = decayBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
            {
                struct TACLine *decayLine = tacRunner->data;
                for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
                {
                    if (get_use_of_operand(decayLine, operandIndex) != U_UNUSED)
                    {
                        struct Type decayedType = *tac_operand_get_type(&decayLine->operands[operandIndex]);
                        type_decay_arrays(&decayedType);
                        decayLine->operands[operandIndex].castAsType = decayedType;
                    }
                }
            }
        }
        break;

        case E_VARIABLE:
        case E_ARGUMENT:
            break;

        case E_STRUCT:
        {
            struct StructEntry *theStruct = thisMember->entry;
            scope_decay_arrays(theStruct->members);
        }
        break;

        case E_ENUM:
            break;
        }
    }
}

void symbol_table_decay_arrays(struct SymbolTable *table)
{
    // scope_decay_arrays(table->globalScope);
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

void symbol_table_move_member_to_parent_scope(struct Scope *scope, struct ScopeMember *toMove, size_t *indexWithinCurrentScope)
{
    scope_insert(scope->parentScope, toMove->name, toMove->entry, toMove->type, toMove->accessibility);
    free(scope->entries->data[*indexWithinCurrentScope]);
    for (size_t entryIndex = *indexWithinCurrentScope; entryIndex < scope->entries->size - 1; entryIndex++)
    {
        scope->entries->data[entryIndex] = scope->entries->data[entryIndex + 1];
    }
    scope->entries->size--;

    // decrement this so that if we are using it as an iterator to entries in the original scope, it is still valid
    (*indexWithinCurrentScope)--;
}

static void collapse_recurse_to_sub_scopes(struct Scope *scope, struct Dictionary *dict, size_t depth)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case E_SCOPE: // recurse to subscopes
        {
            symbol_table_collapse_scopes_rec(thisMember->entry, dict, depth + 1);
        }
        break;

        case E_FUNCTION: // recurse to functions
        {
            if (depth > 0)
            {
                InternalError("Saw function at depth > 0 when collapsing scopes!");
            }
            struct FunctionEntry *thisFunction = thisMember->entry;
            symbol_table_collapse_scopes_rec(thisFunction->mainScope, dict, 0);
        }
        break;

        // skip everything else
        case E_VARIABLE:
        case E_ARGUMENT:
        case E_BASICBLOCK:
            break;

        // ... except structs, which need to be recursed into
        case E_STRUCT:
        {
            struct StructEntry *recursedStruct = thisMember->entry;
            symbol_table_collapse_scopes_rec(recursedStruct->members, dict, 0);
        }
        break;

        case E_ENUM:
            break;
        }
    }
}

static void attempt_operand_mangle(struct TACOperand *operand, struct Scope *scope, struct Dictionary *dict)
{
    // check only TAC operands that both exist and refer to a named variable from the source code (ignore temps etc)
    if ((operand->type.basicType != VT_NULL) &&
        (operand->permutation == VP_STANDARD))
    {
        char *originalName = operand->name.str;

        // bail out early if the variable is not declared within this scope, as we will not need to mangle it
        if (!scope_contains(scope, originalName))
        {
            return;
        }

        // if the declaration for the variable is owned by this scope, ensure that we actually get a variable or argument
        struct VariableEntry *variableToMangle = scope_lookup_var_by_string(scope, originalName);

        // only mangle things which are not string literals
        if (variableToMangle->isStringLiteral == 0)
        {
            // it should not be possible to see a global as being declared here
            if (variableToMangle->isGlobal)
            {
                InternalError("Declaration of variable %s at inner scope %s is marked as a global!", variableToMangle->name, scope->name);
            }
            operand->name.str = symbol_table_mangle_name(scope, dict, originalName);
        }
    }
}

// iterate all TAC lines for all basic blocks within scope, mangling their operands if necessary
static void mangle_block_contents(struct Scope *scope, struct Dictionary *dict)
{
    // second pass: rename basic block operands relevant to the current scope
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case E_SCOPE:
        case E_FUNCTION:
            break;

        case E_BASICBLOCK:
        {
            // rename TAC lines if we are within a function
            if (scope->parentFunction != NULL)
            {
                // go through all TAC lines in this block
                struct BasicBlock *thisBlock = thisMember->entry;
                for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
                {
                    struct TACLine *thisTac = tacRunner->data;
                    for (size_t operandIndex = 0; operandIndex < 4; operandIndex++)
                    {
                        if (get_use_of_operand(thisTac, operandIndex) != U_UNUSED)
                        {
                            attempt_operand_mangle(&thisTac->operands[operandIndex], scope, dict);
                        }
                    }
                }
            }
        }
        break;

        case E_VARIABLE:
        case E_ARGUMENT:
        case E_STRUCT:
        case E_ENUM:
            break;
        }
    }
}

static void move_scope_members_to_parent_scope(struct Scope *scope, struct Dictionary *dict, size_t depth)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case E_SCOPE:
            move_scope_members_to_parent_scope(thisMember->entry, dict, depth + 1);
            break;

        case E_FUNCTION:
        {
            struct FunctionEntry *movedFromFunction = thisMember->entry;
            move_scope_members_to_parent_scope(movedFromFunction->mainScope, dict, 0);
        }
        break;

        case E_BASICBLOCK:
        {
            if (depth > 0 && scope->parentScope != NULL)
            {
                symbol_table_move_member_to_parent_scope(scope, thisMember, &entryIndex);
            }
        }
        break;

        case E_VARIABLE:
        case E_ARGUMENT:
        {
            if (scope->parentScope != NULL)
            {
                struct VariableEntry *variableToMove = thisMember->entry;
                // we will only ever do anything if we are depth >0 or need to kick a global variable up a scope
                if ((depth > 0) || (variableToMove->isGlobal))
                {
                    // mangle all non-global names (want to mangle everything except for string literal names)
                    if (!variableToMove->isGlobal)
                    {
                        thisMember->name = symbol_table_mangle_name(scope, dict, thisMember->name);
                    }
                    symbol_table_move_member_to_parent_scope(scope, thisMember, &entryIndex);
                }
            }
        }
        break;

        case E_STRUCT:
        {
            struct StructEntry *theStruct = thisMember->entry;
            move_scope_members_to_parent_scope(theStruct->members, dict, 0);
        }
        break;

        case E_ENUM:
            break;
        }
    }
}

void symbol_table_collapse_scopes_rec(struct Scope *scope, struct Dictionary *dict, size_t depth)
{
    // first pass: recurse depth-first so everything we do at this call depth will be 100% correct
    collapse_recurse_to_sub_scopes(scope, dict, depth);

    // only rename basic block operands if depth > 0
    // we only want to alter variable names for variables whose names we will mangle as a result of a scope collapse
    if (depth > 0)
    {
        mangle_block_contents(scope, dict);
    }

    // third pass: move nested members to parent scope based on mangled names
    // also moves globals outwards
    move_scope_members_to_parent_scope(scope, dict, depth);
}

void symbol_table_collapse_scopes(struct SymbolTable *table, struct Dictionary *dict)
{
    symbol_table_collapse_scopes_rec(table->globalScope, parseDict, 0);
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
    if (toPrint->type != E_BASICBLOCK || printTac)
    {
        for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
        {
            fprintf(outFile, "\t");
        }
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
        for (struct LinkedListNode *enumMemberRunner = theEnum->members->elements->head; enumMemberRunner != NULL; enumMemberRunner = enumMemberRunner->next)
        {
            struct EnumMember *member = enumMemberRunner->data;
            for (size_t j = 0; j < depth; j++)
            {
                fprintf(outFile, "\t");
            }
            char *memberTypeName = type_get_name(&member->type);
            fprintf(outFile, "%zu:%s (%s)\n", member->numerical, member->name, memberTypeName);
            free(memberTypeName);
        }
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
        if (printTac)
        {
            struct BasicBlock *thisBlock = toPrint->entry;
            fprintf(outFile, "> Basic Block %zu\n", thisBlock->labelNum);
            print_basic_block(thisBlock, depth + 1);
        }
    }
    break;
    }
}

void scope_print(struct Scope *scope, FILE *outFile, size_t depth, bool printTac)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        scope_print_member(thisMember, printTac, depth + 1, outFile);
    }
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
        linked_list_append(scope->parentFunction->BasicBlockList, block);
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
