#include "symtab.h"
#include "codegen_generic.h"
#include "symtab_scope.h"

#include "log.h"

extern struct Dictionary *parseDict;

struct SymbolTable *SymbolTable_new(char *name)
{
    struct SymbolTable *wip = malloc(sizeof(struct SymbolTable));
    wip->name = name;
    wip->globalScope = Scope_new(NULL, "Global", NULL, NULL);
    struct BasicBlock *globalBlock = BasicBlock_new(0);

    // manually insert a basic block for global code so we can give it the custom name of "globalblock"
    Scope_insert(wip->globalScope, "globalblock", globalBlock, e_basicblock, a_public);

    return wip;
}

void SymbolTable_print(struct SymbolTable *table, FILE *outFile, char printTAC)
{
    printf("~~~~~~~~~~~~~\n");
    printf("Symbol Table For %s:\n\n", table->name);
    Scope_print(table->globalScope, outFile, 0, printTAC);
    printf("~~~~~~~~~~~~~\n\n");
}

void Scope_DecayArrays(struct Scope *scope)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case e_scope:
            Scope_DecayArrays(thisMember->entry);
            break;

        case e_function:
        {
            struct FunctionEntry *decayFunction = thisMember->entry;
            Scope_DecayArrays(decayFunction->mainScope);
        }
        break;

        case e_basicblock:
        {
            struct BasicBlock *decayBlock = thisMember->entry;
            for (struct LinkedListNode *tacRunner = decayBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
            {
                struct TACLine *decayLine = tacRunner->data;
                for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
                {
                    if (getUseOfOperand(decayLine, operandIndex) != u_unused)
                    {
                        struct Type decayedType = *TACOperand_GetType(&decayLine->operands[operandIndex]);
                        Type_DecayArrays(&decayedType);
                        decayLine->operands[operandIndex].castAsType = decayedType;
                    }
                }
            }
        }
        break;

        case e_variable:
        case e_argument:
            break;

        case e_struct:
        {
            struct StructEntry *theStruct = thisMember->entry;
            Scope_DecayArrays(theStruct->members);
        }
        break;
        }
    }
}

void SymbolTable_DecayArrays(struct SymbolTable *table)
{
    Scope_DecayArrays(table->globalScope);
}

char *SymbolTable_mangleName(struct Scope *scope, struct Dictionary *dict, char *toMangle)
{
    char *scopeName = scope->name;

    char *mangledName = malloc(strlen(toMangle) + strlen(scopeName) + 2);
    sprintf(mangledName, "%s.%s", scopeName, toMangle);
    char *newName = Dictionary_LookupOrInsert(dict, mangledName);
    free(mangledName);
    return newName;
}

void SymbolTable_moveMemberToParentScope(struct Scope *scope, struct ScopeMember *toMove, size_t *indexWithinCurrentScope)
{
    Scope_insert(scope->parentScope, toMove->name, toMove->entry, toMove->type, toMove->accessibility);
    free(scope->entries->data[*indexWithinCurrentScope]);
    for (size_t entryIndex = *indexWithinCurrentScope; entryIndex < scope->entries->size - 1; entryIndex++)
    {
        scope->entries->data[entryIndex] = scope->entries->data[entryIndex + 1];
    }
    scope->entries->size--;

    // decrement this so that if we are using it as an iterator to entries in the original scope, it is still valid
    (*indexWithinCurrentScope)--;
}

static void collapseRecurseToSubScopes(struct Scope *scope, struct Dictionary *dict, size_t depth)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case e_scope: // recurse to subscopes
        {
            SymbolTable_collapseScopesRec(thisMember->entry, dict, depth + 1);
        }
        break;

        case e_function: // recurse to functions
        {
            if (depth > 0)
            {
                InternalError("Saw function at depth > 0 when collapsing scopes!");
            }
            struct FunctionEntry *thisFunction = thisMember->entry;
            SymbolTable_collapseScopesRec(thisFunction->mainScope, dict, 0);
        }
        break;

        // skip everything else
        case e_variable:
        case e_argument:
        case e_basicblock:
            break;

        // ... except structs, which need to be recursed into
        case e_struct:
        {
            struct StructEntry *recursedStruct = thisMember->entry;
            SymbolTable_collapseScopesRec(recursedStruct->members, dict, 0);
        }
        break;
        }
    }
}

static void attemptOperandMangle(struct TACOperand *operand, struct Scope *scope, struct Dictionary *dict)
{
    // check only TAC operands that both exist and refer to a named variable from the source code (ignore temps etc)
    if ((operand->type.basicType != vt_null) &&
        (operand->permutation == vp_standard))
    {
        char *originalName = operand->name.str;

        // bail out early if the variable is not declared within this scope, as we will not need to mangle it
        if (!Scope_contains(scope, originalName))
        {
            return;
        }

        // if the declaration for the variable is owned by this scope, ensure that we actually get a variable or argument
        struct VariableEntry *variableToMangle = lookupVarByString(scope, originalName);

        // only mangle things which are not string literals
        if (variableToMangle->isStringLiteral == 0)
        {
            // it should not be possible to see a global as being declared here
            if (variableToMangle->isGlobal)
            {
                InternalError("Declaration of variable %s at inner scope %s is marked as a global!", variableToMangle->name, scope->name);
            }
            operand->name.str = SymbolTable_mangleName(scope, dict, originalName);
        }
    }
}

// iterate all TAC lines for all basic blocks within scope, mangling their operands if necessary
static void mangleBlockContents(struct Scope *scope, struct Dictionary *dict)
{
    // second pass: rename basic block operands relevant to the current scope
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case e_scope:
        case e_function:
            break;

        case e_basicblock:
        {
            // rename TAC lines if we are within a function
            if (scope->parentFunction != NULL)
            {
                // go through all TAC lines in this block
                struct BasicBlock *thisBlock = thisMember->entry;
                for (struct LinkedListNode *TACRunner = thisBlock->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
                {
                    struct TACLine *thisTAC = TACRunner->data;
                    for (size_t operandIndex = 0; operandIndex < 4; operandIndex++)
                    {
                        if (getUseOfOperand(thisTAC, operandIndex) != u_unused)
                        {
                            attemptOperandMangle(&thisTAC->operands[operandIndex], scope, dict);
                        }
                    }
                }
            }
        }
        break;

        case e_variable:
        case e_argument:
        case e_struct:
            break;
        }
    }
}

static void moveScopeMembersToParentScope(struct Scope *scope, struct Dictionary *dict, size_t depth)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case e_scope:
            moveScopeMembersToParentScope(thisMember->entry, dict, depth + 1);
            break;

        case e_function:
        {
            struct FunctionEntry *movedFromFunction = thisMember->entry;
            moveScopeMembersToParentScope(movedFromFunction->mainScope, dict, 0);
        }
        break;

        case e_basicblock:
        {
            if (depth > 0 && scope->parentScope != NULL)
            {
                SymbolTable_moveMemberToParentScope(scope, thisMember, &entryIndex);
            }
        }
        break;

        case e_variable:
        case e_argument:
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
                        thisMember->name = SymbolTable_mangleName(scope, dict, thisMember->name);
                    }
                    SymbolTable_moveMemberToParentScope(scope, thisMember, &entryIndex);
                }
            }
        }
        break;

        case e_struct:
        {
            struct StructEntry *theStruct = thisMember->entry;
            moveScopeMembersToParentScope(theStruct->members, dict, 0);
        }
        break;
        }
    }
}

void SymbolTable_collapseScopesRec(struct Scope *scope, struct Dictionary *dict, size_t depth)
{
    // first pass: recurse depth-first so everything we do at this call depth will be 100% correct
    collapseRecurseToSubScopes(scope, dict, depth);

    // only rename basic block operands if depth > 0
    // we only want to alter variable names for variables whose names we will mangle as a result of a scope collapse
    if (depth > 0)
    {
        mangleBlockContents(scope, dict);
    }

    // third pass: move nested members to parent scope based on mangled names
    // also moves globals outwards
    moveScopeMembersToParentScope(scope, dict, depth);
}

void SymbolTable_collapseScopes(struct SymbolTable *table, struct Dictionary *dict)
{
    SymbolTable_collapseScopesRec(table->globalScope, parseDict, 0);
}

void SymbolTable_free(struct SymbolTable *table)
{
    Scope_free(table->globalScope);
    free(table);
}

void VariableEntry_Print(struct VariableEntry *variable, FILE *outFile, size_t depth)
{
    char *typeName = Type_GetName(&variable->type);
    fprintf(outFile, "%s %s\n", typeName, variable->name);
    free(typeName);
}

void Scope_print(struct Scope *scope, FILE *outFile, size_t depth, char printTAC)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];

        if (thisMember->type != e_basicblock || printTAC)
        {
            for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
            {
                fprintf(outFile, "\t");
            }
        }

        switch (thisMember->type)
        {
        case e_argument:
        {
            struct VariableEntry *theArgument = thisMember->entry;
            fprintf(outFile, "> Argument: ");
            VariableEntry_Print(theArgument, outFile, depth);
            for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
            {
                fprintf(outFile, "\t");
            }
            fprintf(outFile, "  - Stack offset: %zd\n", theArgument->stackOffset);
        }
        break;

        case e_variable:
        {
            struct VariableEntry *theVariable = thisMember->entry;
            fprintf(outFile, "> ");
            VariableEntry_Print(theVariable, outFile, depth);
        }
        break;

        case e_struct:
        {
            struct StructEntry *theStruct = thisMember->entry;
            fprintf(outFile, "> Struct %s:\n", thisMember->name);
            for (size_t j = 0; j < depth; j++)
            {
                fprintf(outFile, "\t");
            }
            fprintf(outFile, "  - Size: %zu bytes\n", theStruct->totalSize);
            Scope_print(theStruct->members, outFile, depth + 1, printTAC);
        }
        break;

        case e_function:
        {
            struct FunctionEntry *theFunction = thisMember->entry;
            char *returnTypeName = Type_GetName(&theFunction->returnType);
            if (theFunction->methodOf != NULL)
            {
                fprintf(outFile, "> Method %s.", theFunction->methodOf->name);
            }
            else
            {
                fprintf(outFile, "> Function ");
            }
            fprintf(outFile, "%s (returns %s) (defined: %d)\n", thisMember->name, returnTypeName, theFunction->isDefined);
            free(returnTypeName);
            Scope_print(theFunction->mainScope, outFile, depth + 1, printTAC);
        }
        break;

        case e_scope:
        {
            struct Scope *theScope = thisMember->entry;
            fprintf(outFile, "> Subscope %s\n", thisMember->name);
            Scope_print(theScope, outFile, depth + 1, printTAC);
        }
        break;

        case e_basicblock:
        {
            if (printTAC)
            {
                struct BasicBlock *thisBlock = thisMember->entry;
                fprintf(outFile, "> Basic Block %zu\n", thisBlock->labelNum);
                printBasicBlock(thisBlock, depth + 1);
            }
        }
        break;
        }
    }
}

void Scope_addBasicBlock(struct Scope *scope, struct BasicBlock *block)
{
    const u8 basicBlockNameStrSize = 10; // TODO: manage this better
    char *blockName = malloc(basicBlockNameStrSize);
    sprintf(blockName, "Block%zu", block->labelNum);
    Scope_insert(scope, Dictionary_LookupOrInsert(parseDict, blockName), block, e_basicblock, a_public);
    free(blockName);

    if (scope->parentFunction != NULL)
    {
        LinkedList_Append(scope->parentFunction->BasicBlockList, block);
    }
}

/*
 * AST walk and symbol table generation functions
 */

// scrape down a chain of adjacent sibling star tokens, expecting something at the bottom
size_t scrapePointers(struct AST *pointerAST, struct AST **resultDestination)
{
    size_t dereferenceDepth = 0;
    pointerAST = pointerAST->sibling;

    while ((pointerAST != NULL) && (pointerAST->type == t_dereference))
    {
        dereferenceDepth++;
        pointerAST = pointerAST->sibling;
    }

    *resultDestination = pointerAST;
    return dereferenceDepth;
}
