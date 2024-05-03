#include "linearizer.h"
#include "codegen_generic.h"
#include "linearizer_generic.h"

#include "log.h"
#include "symtab.h"

#include <ctype.h>

// pre-refactoring - 2131 lines

/*
 * These functions walk the AST and convert it to three-address code
 */
struct TempList *temps;
struct Dictionary *typeDict;
extern struct Dictionary *parseDict;
const u8 TYPE_DICT_SIZE = 10;
struct SymbolTable *walkProgram(struct AST *program)
{
    typeDict = Dictionary_New(TYPE_DICT_SIZE, (void *(*)(void *))Type_Duplicate, (size_t(*)(void *))Type_Hash, (ssize_t(*)(void *, void *))Type_Compare, (void (*)(void *))Type_Free);
    struct SymbolTable *programTable = SymbolTable_new("Program");
    struct BasicBlock *globalBlock = Scope_lookup(programTable->globalScope, "globalblock")->entry;
    struct BasicBlock *asmBlock = BasicBlock_new(1);
    Scope_addBasicBlock(programTable->globalScope, asmBlock);
    temps = TempList_New();

    size_t globalTACIndex = 0;
    size_t globalTempNum = 0;

    struct AST *programRunner = program;
    while (programRunner != NULL)
    {
        switch (programRunner->type)
        {
        case t_variable_declaration:
            // walkVariableDeclaration sets isGlobal for us by checking if there is no parent scope
            walkVariableDeclaration(programRunner, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum, 0);
            break;

        case t_extern:
        {
            struct VariableEntry *declaredVariable = walkVariableDeclaration(programRunner->child, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum, 0);
            declaredVariable->isExtern = 1;
        }
        break;

        case t_class:
        {
            walkClassDeclaration(programRunner, globalBlock, programTable->globalScope);
            break;
        }
        break;

        case t_assign:
            walkAssignment(programRunner, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum);
            break;

        case t_impl:
            walkImplementationBlock(programRunner, programTable->globalScope);
            break;

        case t_fun:
            walkFunctionDeclaration(programRunner, programTable->globalScope, NULL);
            break;

        // ignore asm blocks
        case t_asm:
            walkAsmBlock(programRunner, asmBlock, programTable->globalScope, &globalTACIndex, &globalTempNum);
            break;

        default:
            InternalError("Malformed AST in walkProgram: got %s with type %s",
                          programRunner->value,
                          getTokenName(programRunner->type));
            break;
        }
        programRunner = programRunner->sibling;
    }

    return programTable;
}

void walkTypeName(struct AST *tree, struct Scope *scope, struct Type *populateTypeTo)
{
    LogTree(LOG_DEBUG, tree, "walkTypeName");
    if (tree->type != t_type_name)
    {
        InternalError("Wrong AST (%s) passed to walkTypeName!", getTokenName(tree->type));
    }

    memset(populateTypeTo, 0, sizeof(struct Type));

    struct AST *classNameTree = NULL;
    enum basicTypes basicType = vt_null;
    char *className = NULL;

    switch (tree->child->type)
    {
    case t_any:
        basicType = vt_any;
        break;

    case t_u8:
        basicType = vt_u8;
        break;

    case t_u16:
        basicType = vt_u16;
        break;

    case t_u32:
        basicType = vt_u32;
        break;

    case t_u64:
        basicType = vt_u64;
        break;

    case t_identifier:
        basicType = vt_class;

        classNameTree = tree->child;
        if (classNameTree->type != t_identifier)
        {
            LogTree(ERROR_INTERNAL,
                    classNameTree,
                    "Malformed AST seen in declaration!\nExpected class name as child of \"class\", saw %s (%s)!",
                    classNameTree->value,
                    getTokenName(classNameTree->type));
        }
        className = classNameTree->value;
        break;

    default:
        LogTree(LOG_FATAL, tree, "Malformed AST seen in declaration!");
    }

    struct AST *declaredArray = NULL;
    Type_SetBasicType(populateTypeTo, basicType, className, scrapePointers(tree->child, &declaredArray));

    // if declaring something with the 'any' type, make sure it's only as a pointer (as its intended use is to point to unstructured data)
    if (populateTypeTo->basicType == vt_array || populateTypeTo->basicType == vt_any)
    {
        struct Type anyCheckRunner = *populateTypeTo;
        while (anyCheckRunner.basicType == vt_array)
        {
            anyCheckRunner = *anyCheckRunner.array.type;
        }

        if ((anyCheckRunner.pointerLevel == 0) && (anyCheckRunner.basicType == vt_any))
        {
            if (populateTypeTo->basicType == vt_array)
            {
                LogTree(LOG_FATAL, declaredArray, "Use of the type 'any' in arrays is forbidden!\n'any' is meant to represent unstructured data as a pointer type only\n(declare as 'any *', 'any **', etc...)");
            }
            else
            {
                LogTree(LOG_FATAL, tree->child, "Use of the type 'any' without indirection is forbidden!\n'any' is meant to represent unstructured data as a pointer type only\n(declare as 'any *', 'any **', etc...)");
            }
        }
    }

    // don't allow declaration of variables of undeclared class or array of undeclared class (except pointers)
    if ((populateTypeTo->basicType == vt_class) && (populateTypeTo->pointerLevel == 0))
    {
        // the lookup will bail out if an attempt is made to use an undeclared class
        lookupClass(scope, classNameTree);
    }

    // if we are declaring an array, set the string with the size as the second operand
    if (declaredArray != NULL)
    {
        if (declaredArray->type != t_array_index)
        {
            LogTree(LOG_FATAL, declaredArray, "Unexpected AST at end of pointer declarations!");
        }
        char *arraySizeString = declaredArray->child->value;
        // TODO: abstract this
        int declaredArraySize = atoi(arraySizeString);

        struct Type *arrayedType = Dictionary_LookupOrInsert(typeDict, populateTypeTo);

        // TODO: multidimensional array declarations
        populateTypeTo->basicType = vt_array;
        populateTypeTo->array.size = declaredArraySize;
        populateTypeTo->array.type = arrayedType;
        populateTypeTo->array.initializeArrayTo = NULL;
    }
}

struct VariableEntry *walkVariableDeclaration(struct AST *tree,
                                              struct BasicBlock *block,
                                              struct Scope *scope,
                                              const size_t *TACIndex,
                                              const size_t *tempNum,
                                              char isArgument)
{
    LogTree(LOG_DEBUG, tree, "walkVariableDeclaration");

    if (tree->type != t_variable_declaration)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkVariableDeclaration!", getTokenName(tree->type));
    }

    struct Type declaredType;

    /* 'class' trees' children are the class name
     * other variables' children are the pointer or variable name
     * so we need to start at tree->child for non-class or tree->child->sibling for classes
     */

    if (tree->child->type != t_type_name)
    {
        LogTree(LOG_FATAL, tree->child, "Malformed AST seen in declaration!");
    }

    walkTypeName(tree->child, scope, &declaredType);

    // automatically set as a global if there is no parent scope (declaring at the outermost scope)
    struct VariableEntry *declaredVariable = createVariable(scope,
                                                            tree->child->sibling,
                                                            &declaredType,
                                                            (u8)(scope->parentScope == NULL),
                                                            *TACIndex,
                                                            isArgument);

    return declaredVariable;
}

void walkArgumentDeclaration(struct AST *tree,
                             struct BasicBlock *block,
                             size_t *TACIndex,
                             size_t *tempNum,
                             struct FunctionEntry *fun)
{
    LogTree(LOG_DEBUG, tree, "walkArgumentDeclaration");

    struct VariableEntry *declaredArgument = walkVariableDeclaration(tree, block, fun->mainScope, TACIndex, tempNum, 1);

    Stack_Push(fun->arguments, declaredArgument);
}

void verifyFunctionSignatures(struct AST *tree, struct FunctionEntry *existingFunc, struct FunctionEntry *parsedFunc)
{
    // nothing to do if no existing function
    if (existingFunc == NULL)
    {
        return;
    }
    // check that if a prototype declaration exists, that our parsed declaration matches it exactly
    u8 mismatch = 0;

    if ((Type_Compare(&parsedFunc->returnType, &existingFunc->returnType)))
    {
        mismatch = 1;
    }

    // ensure we have both the same number of bytes of arguments and same number of arguments
    if (!mismatch &&
        (existingFunc->argStackSize == parsedFunc->argStackSize) &&
        (existingFunc->arguments->size == parsedFunc->arguments->size))
    {
        // if we have same number of bytes and same number, ensure everything is exactly the same
        for (size_t argIndex = 0; argIndex < existingFunc->arguments->size; argIndex++)
        {
            struct VariableEntry *existingArg = existingFunc->arguments->data[argIndex];
            struct VariableEntry *parsedArg = parsedFunc->arguments->data[argIndex];
            // ensure all arguments in order have same name, type, indirection level
            if ((strcmp(existingArg->name, parsedArg->name) != 0) ||
                (Type_Compare(&existingArg->type, &parsedArg->type)))
            {
                mismatch = 1;
                break;
            }
        }
    }
    else
    {
        mismatch = 1;
    }

    if (mismatch)
    {
        printf("\nConflicting declarations of function:\n");

        char *existingReturnType = Type_GetName(&existingFunc->returnType);
        printf("\t%s %s(", existingReturnType, existingFunc->name);
        free(existingReturnType);
        for (size_t argIndex = 0; argIndex < existingFunc->arguments->size; argIndex++)
        {
            struct VariableEntry *existingArg = existingFunc->arguments->data[argIndex];

            char *argType = Type_GetName(&existingArg->type);
            printf("%s %s", argType, existingArg->name);
            free(argType);

            if (argIndex < existingFunc->arguments->size - 1)
            {
                printf(", ");
            }
            else
            {
                printf(")");
            }
        }
        char *parsedReturnType = Type_GetName(&parsedFunc->returnType);
        printf("\n\t%s %s(", parsedReturnType, parsedFunc->name);
        free(parsedReturnType);
        for (size_t argIndex = 0; argIndex < parsedFunc->arguments->size; argIndex++)
        {
            struct VariableEntry *parsedArg = parsedFunc->arguments->data[argIndex];

            char *argType = Type_GetName(&parsedArg->type);
            printf("%s %s", argType, parsedArg->name);
            free(argType);

            if (argIndex < parsedFunc->arguments->size - 1)
            {
                printf(", ");
            }
            else
            {
                printf(")");
            }
        }
        printf("\n");

        LogTree(LOG_FATAL, tree, " ");
    }
}

struct FunctionEntry *walkFunctionDeclaration(struct AST *tree,
                                              struct Scope *scope,
                                              struct ClassEntry *methodOf)
{
    LogTree(LOG_DEBUG, tree, "walkFunctionDeclaration");

    if (tree->type != t_fun)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkFunctionDeclaration!", getTokenName(tree->type));
    }

    // skip past the argumnent declarations to the return type declaration
    struct AST *returnTypeTree = tree->child;

    // functions return nothing in the default case
    struct Type returnType;
    memset(&returnType, 0, sizeof(struct Type));

    struct AST *functionNameTree = NULL;

    // if the function returns something, its return type will be the first child of the 'fun' token
    if (returnTypeTree->type == t_type_name)
    {
        walkTypeName(returnTypeTree, scope, &returnType);

        // if we are returning a class, ensure that we're returning some sort of pointer, not a whole object
        // TODO: testing for error messages, printing of exact types causing the error
        if ((returnType.basicType == vt_class) && (returnType.pointerLevel == 0))
        {
            // use tree->child to get the original returnTypeTree AST as scrapePointers may have modified it
            LogTree(LOG_FATAL, tree->child, "Return of class object types is not supported!");
        }
        else if (returnType.basicType == vt_array)
        {
            char *arrayTypeName = Type_GetName(&returnType);
            LogTree(LOG_FATAL, tree->child, "Return of array object types (%s) is not supported!", arrayTypeName);
        }

        functionNameTree = returnTypeTree->sibling;
    }
    else
    {
        // there actually is no return type tree, we just go directly to argument declarations
        functionNameTree = returnTypeTree;
    }

    // child is the lparen, function name is the child of the lparen
    struct ScopeMember *lookedUpFunction = Scope_lookup(scope, functionNameTree->value);
    struct FunctionEntry *parsedFunc = NULL;
    struct FunctionEntry *existingFunc = NULL;
    struct FunctionEntry *returnedFunc = NULL;

    if (lookedUpFunction != NULL)
    {
        existingFunc = lookedUpFunction->entry;
        returnedFunc = existingFunc;
        parsedFunc = FunctionEntry_new(scope, functionNameTree, &returnType);
    }
    else
    {
        parsedFunc = createFunction(scope, functionNameTree, &returnType);
        parsedFunc->mainScope->parentScope = scope;
        returnedFunc = parsedFunc;
    }

    struct AST *argumentRunner = functionNameTree->sibling;
    size_t TACIndex = 0;
    size_t tempNum = 0;
    struct BasicBlock *block = BasicBlock_new(0);
    while ((argumentRunner != NULL) && (argumentRunner->type != t_compound_statement) && (argumentRunner->type != t_asm))
    {
        switch (argumentRunner->type)
        {
        // looking at argument declarations
        case t_variable_declaration:
        {
            walkArgumentDeclaration(argumentRunner, block, &TACIndex, &tempNum, parsedFunc);
        }
        break;

        case t_self:
        {
            if (methodOf == NULL)
            {
                LogTree(LOG_FATAL, argumentRunner, "Malformed AST within function declaration - saw self when methodOf == NULL");
            }
            struct Type selfType;
            Type_Init(&selfType);
            Type_SetBasicType(&selfType, vt_class, methodOf->name, 1);
            struct VariableEntry *selfArgument = createVariable(parsedFunc->mainScope, argumentRunner, &selfType, 0, 0, 1);

            Stack_Push(parsedFunc->arguments, selfArgument);
        }
        break;

        default:
            InternalError("Malformed AST within function - expected function name and main scope only!\nMalformed node was of type %s with value [%s]", getTokenName(argumentRunner->type), argumentRunner->value);
        }
        argumentRunner = argumentRunner->sibling;
    }

    while (parsedFunc->argStackSize % STACK_ALIGN_BYTES)
    {
        parsedFunc->argStackSize++;
    }

    verifyFunctionSignatures(tree, existingFunc, parsedFunc);

    // free the basic block we used to walk declarations of arguments
    BasicBlock_free(block);

    struct AST *definition = argumentRunner;
    if (definition != NULL)
    {
        if (existingFunc != NULL)
        {
            FunctionEntry_free(parsedFunc);
            existingFunc->isDefined = 1;
            walkFunctionDefinition(definition, existingFunc);
        }
        else
        {
            parsedFunc->isDefined = 1;
            walkFunctionDefinition(definition, parsedFunc);
        }
    }

    return returnedFunc;
}

void walkFunctionDefinition(struct AST *tree,
                            struct FunctionEntry *fun)
{
    LogTree(LOG_DEBUG, tree, "walkFunctionDefinition");

    if ((tree->type != t_compound_statement) && (tree->type != t_asm))
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkFunctionDefinition!", getTokenName(tree->type));
    }

    size_t TACIndex = 0;
    size_t tempNum = 0;
    ssize_t labelNum = 1;
    struct BasicBlock *block = BasicBlock_new(0);
    Scope_addBasicBlock(fun->mainScope, block);

    if (tree->type == t_compound_statement)
    {
        walkScope(tree, block, fun->mainScope, &TACIndex, &tempNum, &labelNum, -1);
    }
    else
    {
        fun->isAsmFun = 1;
        walkAsmBlock(tree, block, fun->mainScope, &TACIndex, &tempNum);
    }
}

void walkMethod(struct AST *tree,
                struct ClassEntry *class)
{
    Log(LOG_DEBUG, "walkMethod", tree->sourceFile, tree->sourceLine, tree->sourceCol);

    if (tree->type != t_fun)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkMethod!", getTokenName(tree->type));
    }

    struct FunctionEntry *walkedMethod = walkFunctionDeclaration(tree, class->members, class);
    if (walkedMethod->arguments->size > 0)
    {
        struct VariableEntry *firstArg = walkedMethod->arguments->data[0];

        if ((firstArg->type.basicType == vt_class) && (strcmp(firstArg->type.nonArray.complexType.name, class->name) == 0))
        {
            if (strcmp(firstArg->name, "self") == 0)
            {
                walkedMethod->methodOf = class;
            }
        }
    }
}

void walkImplementationBlock(struct AST *tree, struct Scope *scope)
{
    Log(LOG_DEBUG, "walkImplementation", tree->sourceFile, tree->sourceLine, tree->sourceCol);

    if (tree->type != t_impl)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkImplementation!", getTokenName(tree->type));
    }

    struct AST *implementedClassTree = tree->child;
    if (implementedClassTree->type != t_identifier)
    {
        LogTree(LOG_FATAL, implementedClassTree, "Malformed AST seen in walkImplementation!");
    }

    struct ClassEntry *implementedClass = lookupClass(scope, implementedClassTree);

    struct AST *implementationRunner = implementedClassTree->sibling;
    while (implementationRunner != NULL)
    {
        switch (implementationRunner->type)
        {
        case t_fun:
            walkMethod(implementationRunner, implementedClass);
            break;

        default:
            LogTree(LOG_FATAL, implementationRunner, "Malformed AST seen %s (%s) in walkImplementation!", getTokenName(implementationRunner->type), implementationRunner->value);
        }
        implementationRunner = implementationRunner->sibling;
    }
}

void walkClassDeclaration(struct AST *tree,
                          struct BasicBlock *block,
                          struct Scope *scope)
{
    LogTree(LOG_DEBUG, tree, "walkClassDeclaration");

    if (tree->type != t_class)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkClassDefinition!", getTokenName(tree->type));
    }
    size_t dummyNum = 0;

    struct ClassEntry *declaredClass = createClass(scope, tree->child->value);

    struct AST *classBody = tree->child->sibling;

    if (classBody->type != t_class_body)
    {
        LogTree(LOG_FATAL, tree, "Malformed AST seen in walkClassDefinition!");
    }

    struct AST *classBodyRunner = classBody->child;
    while (classBodyRunner != NULL)
    {
        switch (classBodyRunner->type)
        {
        case t_variable_declaration:
        {
            struct VariableEntry *declaredMember = walkVariableDeclaration(classBodyRunner, block, declaredClass->members, &dummyNum, &dummyNum, 0);
            assignOffsetToMemberVariable(declaredClass, declaredMember);
        }
        break;

        default:
            LogTree(LOG_FATAL, classBodyRunner, "Wrong AST (%s) seen in body of class definition!", getTokenName(classBodyRunner->type));
        }

        classBodyRunner = classBodyRunner->sibling;
    }
}

void walkStatement(struct AST *tree,
                   struct BasicBlock **blockP,
                   struct Scope *scope,
                   size_t *TACIndex,
                   size_t *tempNum,
                   ssize_t *labelNum,
                   ssize_t controlConvergesToLabel)
{
    LogTree(LOG_DEBUG, tree, "walkStatement");

    switch (tree->type)
    {
    case t_variable_declaration:
        walkVariableDeclaration(tree, *blockP, scope, TACIndex, tempNum, 0);
        break;

    case t_extern:
        LogTree(LOG_FATAL, tree, "'extern' is only allowed at the global scope.");
        break;

    case t_assign:
        walkAssignment(tree, *blockP, scope, TACIndex, tempNum);
        break;

    case t_plus_equals:
    case t_minus_equals:
    case t_times_equals:
    case t_divide_equals:
    case t_modulo_equals:
    case t_bitwise_and_equals:
    case t_bitwise_or_equals:
    case t_bitwise_xor_equals:
    case t_lshift_equals:
    case t_rshift_equals:
        walkArithmeticAssignment(tree, *blockP, scope, TACIndex, tempNum);
        break;

    case t_while:
    {
        struct BasicBlock *afterWhileBlock = BasicBlock_new((*labelNum)++);
        walkWhileLoop(tree, *blockP, scope, TACIndex, tempNum, labelNum, afterWhileBlock->labelNum);
        *blockP = afterWhileBlock;
        Scope_addBasicBlock(scope, afterWhileBlock);
    }
    break;

    case t_if:
    {
        struct BasicBlock *afterIfBlock = BasicBlock_new((*labelNum)++);
        walkIfStatement(tree, *blockP, scope, TACIndex, tempNum, labelNum, afterIfBlock->labelNum);
        *blockP = afterIfBlock;
        Scope_addBasicBlock(scope, afterIfBlock);
    }
    break;

    case t_function_call:
        walkFunctionCall(tree, *blockP, scope, TACIndex, tempNum, NULL);
        break;

    case t_method_call:
        walkMethodCall(tree, *blockP, scope, TACIndex, tempNum, NULL);
        break;

    // subscope
    case t_compound_statement:
    {
        // TODO: is there a bug here for simple scopes within code (not attached to if/while/etc... statements? TAC dump for the scopes test seems to indicate so?)
        struct Scope *subScope = Scope_createSubScope(scope);
        struct BasicBlock *afterSubScopeBlock = BasicBlock_new((*labelNum)++);
        walkScope(tree, *blockP, subScope, TACIndex, tempNum, labelNum, afterSubScopeBlock->labelNum);
        *blockP = afterSubScopeBlock;
        Scope_addBasicBlock(scope, afterSubScopeBlock);
    }
    break;

    case t_return:
    {
        struct TACLine *returnLine = newTACLine(tt_return, tree);
        if (tree->child != NULL)
        {
            walkSubExpression(tree->child, *blockP, scope, TACIndex, tempNum, &returnLine->operands[0]);
        }

        BasicBlock_append(*blockP, returnLine, TACIndex);

        if (tree->sibling != NULL)
        {
            LogTree(LOG_FATAL, tree->sibling, "Code after return statement is unreachable!");
        }
    }
    break;

    case t_asm:
        walkAsmBlock(tree, *blockP, scope, TACIndex, tempNum);
        break;

    default:
        LogTree(LOG_FATAL, tree, "Unexpected AST type (%s - %s) seen in walkStatement!", getTokenName(tree->type), tree->value);
    }
}

void walkScope(struct AST *tree,
               struct BasicBlock *block,
               struct Scope *scope,
               size_t *TACIndex,
               size_t *tempNum,
               ssize_t *labelNum,
               ssize_t controlConvergesToLabel)
{
    LogTree(LOG_DEBUG, tree, "walkScope");

    if (tree->type != t_compound_statement)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkScope!", getTokenName(tree->type));
    }

    struct AST *scopeRunner = tree->child;
    while (scopeRunner != NULL)
    {
        walkStatement(scopeRunner, &block, scope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        scopeRunner = scopeRunner->sibling;
    }

    if (controlConvergesToLabel > 0)
    {
        struct TACLine *controlConvergeJmp = newTACLine(tt_jmp, tree);
        controlConvergeJmp->operands[0].name.val = controlConvergesToLabel;
        BasicBlock_append(block, controlConvergeJmp, TACIndex);
    }
}

struct BasicBlock *walkLogicalOperator(struct AST *tree,
                                       struct BasicBlock *block,
                                       struct Scope *scope,
                                       size_t *TACIndex,
                                       size_t *tempNum,
                                       ssize_t *labelNum,
                                       ssize_t falseJumpLabelNum)
{
    LogTree(LOG_DEBUG, tree, "walkLogicalOperator");

    switch (tree->type)
    {
    case t_logical_and:
    {
        // if either condition is false, immediately jump to the false label
        block = walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);
        block = walkConditionCheck(tree->child->sibling, block, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);
    }
    break;

    case t_logical_or:
    {
        // this block will only be hit if the first condition comes back false
        struct BasicBlock *checkSecondConditionBlock = BasicBlock_new((*labelNum)++);
        Scope_addBasicBlock(scope, checkSecondConditionBlock);

        // this is the block in which execution will end up if the condition is true
        struct BasicBlock *trueBlock = BasicBlock_new((*labelNum)++);
        Scope_addBasicBlock(scope, trueBlock);
        block = walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, labelNum, checkSecondConditionBlock->labelNum);

        // if we pass the first condition (don't jump to checkSecondConditionBlock), short-circuit directly to the true block
        struct TACLine *firstConditionTrueJump = newTACLine(tt_jmp, tree->child);
        firstConditionTrueJump->operands[0].name.val = trueBlock->labelNum;
        BasicBlock_append(block, firstConditionTrueJump, TACIndex);

        // walk the second condition to checkSecondConditionBlock
        block = walkConditionCheck(tree->child->sibling, checkSecondConditionBlock, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);

        // jump from whatever block the second condition check ends up in (passing path) to our block
        // this ensures that regardless of which condition is true (first or second) execution always end up in the same block
        struct TACLine *secondConditionTrueJump = newTACLine(tt_jmp, tree->child->sibling);
        secondConditionTrueJump->operands[0].name.val = trueBlock->labelNum;
        BasicBlock_append(block, secondConditionTrueJump, TACIndex);

        block = trueBlock;
    }
    break;

    case t_logical_not:
    {
        // walkConditionCheck already does everything we need it to
        // so just create a block representing the opposite of the condition we are testing
        // then, tell walkConditionCheck to go there if our subcondition is false (!subcondition is true)
        struct BasicBlock *inverseConditionBlock = BasicBlock_new((*labelNum)++);
        Scope_addBasicBlock(scope, inverseConditionBlock);

        block = walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, labelNum, inverseConditionBlock->labelNum);

        // subcondition is true (!subcondition is false), then control flow should end up at the original conditionFalseJump destination
        struct TACLine *conditionFalseJump = newTACLine(tt_jmp, tree->child);
        conditionFalseJump->operands[0].name.val = falseJumpLabelNum;
        BasicBlock_append(block, conditionFalseJump, TACIndex);

        // return the tricky block we created to be jumped to when our subcondition is false, or that the condition we are linearizing at this level is true
        block = inverseConditionBlock;
    }
    break;

    default:
        InternalError("Logical operator %s (%s) not supported yet",
                      getTokenName(tree->type),
                      tree->value);
    }

    return block;
}

struct BasicBlock *walkConditionCheck(struct AST *tree,
                                      struct BasicBlock *block,
                                      struct Scope *scope,
                                      size_t *TACIndex,
                                      size_t *tempNum,
                                      ssize_t *labelNum,
                                      ssize_t falseJumpLabelNum)
{
    LogTree(LOG_DEBUG, tree, "walkConditionCheck");

    struct TACLine *condFalseJump = newTACLine(tt_jmp, tree);
    condFalseJump->operands[0].name.val = falseJumpLabelNum;

    // switch once to decide the jump type
    switch (tree->type)
    {
    case t_equals:
        condFalseJump->operation = tt_bne;
        break;

    case t_not_equals:
        condFalseJump->operation = tt_beq;
        break;

    case t_less_than:
        condFalseJump->operation = tt_bgeu;
        break;

    case t_greater_than:
        condFalseJump->operation = tt_bleu;
        break;

    case t_less_than_equals:
        condFalseJump->operation = tt_bgtu;
        break;

    case t_greater_than_equals:
        condFalseJump->operation = tt_bltu;
        break;

    case t_logical_and:
    case t_logical_or:
    case t_logical_not:
        block = walkLogicalOperator(tree, block, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);
        break;

    default:
        condFalseJump->operation = tt_bne;
        break;
    }

    // switch a second time to actually walk the condition
    switch (tree->type)
    {
    // arithmetic comparisons
    case t_equals:
    case t_not_equals:
    case t_less_than:
    case t_greater_than:
    case t_less_than_equals:
    case t_greater_than_equals:
        // standard operators (==, !=, <, >, <=, >=)
        {
            switch (tree->child->type)
            {
            case t_logical_and:
            case t_logical_or:
            case t_logical_not:
                LogTree(LOG_FATAL, tree->child, "Use of comparison operators on results of logical operators is not supported!");
                break;

            default:
                walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &condFalseJump->operands[1]);
                break;
            }

            switch (tree->child->sibling->type)
            {
            case t_logical_and:
            case t_logical_or:
            case t_logical_not:
                LogTree(LOG_FATAL, tree->child->sibling, "Use of comparison operators on results of logical operators is not supported!");
                break;

            default:
                walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &condFalseJump->operands[2]);
                break;
            }
        }
        break;

    case t_logical_and:
    case t_logical_or:
    case t_logical_not:
        free(condFalseJump);
        condFalseJump = NULL;
        break;

    case t_identifier:
    case t_add:
    case t_subtract:
    case t_multiply:
    case t_divide:
    case t_modulo:
    case t_lshift:
    case t_rshift:
    case t_bitwise_and:
    case t_bitwise_or:
    case t_bitwise_not:
    case t_bitwise_xor:
    case t_dereference:
    case t_address_of:
    case t_cast:
    case t_dot:
    case t_function_call:
    {
        condFalseJump->operation = tt_beq;
        walkSubExpression(tree, block, scope, TACIndex, tempNum, &condFalseJump->operands[1]);

        condFalseJump->operands[2].type.basicType = vt_u8;
        condFalseJump->operands[2].permutation = vp_literal;
        condFalseJump->operands[2].name.str = "0";
    }
    break;

    default:
    {
        InternalError("Comparison operator %s (%s) not supported yet",
                      getTokenName(tree->type),
                      tree->value);
    }
    break;
    }

    if (condFalseJump != NULL)
    {
        BasicBlock_append(block, condFalseJump, TACIndex);
    }
    return block;
}

void walkWhileLoop(struct AST *tree,
                   struct BasicBlock *block,
                   struct Scope *scope,
                   size_t *TACIndex,
                   size_t *tempNum,
                   ssize_t *labelNum,
                   ssize_t controlConvergesToLabel)
{
    LogTree(LOG_DEBUG, tree, "walkWhileLoop");

    if (tree->type != t_while)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkWhileLoop!", getTokenName(tree->type));
    }

    struct BasicBlock *beforeWhileBlock = block;

    struct TACLine *enterWhileJump = newTACLine(tt_jmp, tree);
    enterWhileJump->operands[0].name.val = *labelNum;
    BasicBlock_append(beforeWhileBlock, enterWhileJump, TACIndex);

    // create a subscope from which we will work
    struct Scope *whileScope = Scope_createSubScope(scope);
    struct BasicBlock *whileBlock = BasicBlock_new((*labelNum)++);
    Scope_addBasicBlock(whileScope, whileBlock);

    struct TACLine *whileDo = newTACLine(tt_do, tree);
    BasicBlock_append(whileBlock, whileDo, TACIndex);

    whileBlock = walkConditionCheck(tree->child, whileBlock, whileScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);

    ssize_t endWhileLabel = (*labelNum)++;

    struct AST *whileBody = tree->child->sibling;
    if (whileBody->type == t_compound_statement)
    {
        walkScope(whileBody, whileBlock, whileScope, TACIndex, tempNum, labelNum, endWhileLabel);
    }
    else
    {
        walkStatement(whileBody, &whileBlock, whileScope, TACIndex, tempNum, labelNum, endWhileLabel);
    }

    struct TACLine *whileLoopJump = newTACLine(tt_jmp, tree);
    whileLoopJump->operands[0].name.val = enterWhileJump->operands[0].name.val;

    block = BasicBlock_new(endWhileLabel);
    Scope_addBasicBlock(scope, block);

    struct TACLine *whileEndDo = newTACLine(tt_enddo, tree);
    BasicBlock_append(block, whileLoopJump, TACIndex);
    BasicBlock_append(block, whileEndDo, TACIndex);
}

void walkIfStatement(struct AST *tree,
                     struct BasicBlock *block,
                     struct Scope *scope,
                     size_t *TACIndex,
                     size_t *tempNum,
                     ssize_t *labelNum,
                     ssize_t controlConvergesToLabel)
{
    LogTree(LOG_DEBUG, tree, "walkIfStatement");

    if (tree->type != t_if)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkIfStatement!", getTokenName(tree->type));
    }

    // if we have an else block
    if (tree->child->sibling->sibling != NULL)
    {
        ssize_t elseLabel = (*labelNum)++;
        block = walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, labelNum, elseLabel);

        struct Scope *ifScope = Scope_createSubScope(scope);
        struct BasicBlock *ifBlock = BasicBlock_new((*labelNum)++);
        Scope_addBasicBlock(ifScope, ifBlock);

        struct TACLine *enterIfJump = newTACLine(tt_jmp, tree);
        enterIfJump->operands[0].name.val = ifBlock->labelNum;
        BasicBlock_append(block, enterIfJump, TACIndex);

        struct AST *ifBody = tree->child->sibling;
        if (ifBody->type == t_compound_statement)
        {
            walkScope(ifBody, ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
        else
        {
            walkStatement(ifBody, &ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }

        struct Scope *elseScope = Scope_createSubScope(scope);
        struct BasicBlock *elseBlock = BasicBlock_new(elseLabel);
        Scope_addBasicBlock(elseScope, elseBlock);

        struct AST *elseBody = tree->child->sibling->sibling;
        if (elseBody->type == t_compound_statement)
        {
            walkScope(elseBody, elseBlock, elseScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
        else
        {
            walkStatement(elseBody, &elseBlock, elseScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
    }
    // no else block
    else
    {
        block = walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, labelNum, controlConvergesToLabel);

        struct Scope *ifScope = Scope_createSubScope(scope);
        struct BasicBlock *ifBlock = BasicBlock_new((*labelNum)++);
        Scope_addBasicBlock(ifScope, ifBlock);

        struct TACLine *enterIfJump = newTACLine(tt_jmp, tree);
        enterIfJump->operands[0].name.val = ifBlock->labelNum;
        BasicBlock_append(block, enterIfJump, TACIndex);

        struct AST *ifBody = tree->child->sibling;
        if (ifBody->type == t_compound_statement)
        {
            walkScope(ifBody, ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
        else
        {
            walkStatement(ifBody, &ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
    }
}

void walkAssignment(struct AST *tree,
                    struct BasicBlock *block,
                    struct Scope *scope,
                    size_t *TACIndex,
                    size_t *tempNum)
{
    LogTree(LOG_DEBUG, tree, "walkAssignment");

    if (tree->type != t_assign)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkAssignment!", getTokenName(tree->type));
    }

    struct AST *lhs = tree->child;
    struct AST *rhs = tree->child->sibling;

    // don't increment the index until after we deal with nested expressions
    struct TACLine *assignment = newTACLine(tt_assign, tree);

    struct TACOperand assignedValue;
    memset(&assignedValue, 0, sizeof(struct TACOperand));

    // walk the RHS of the assignment as a subexpression and save the operand for later
    walkSubExpression(rhs, block, scope, TACIndex, tempNum, &assignedValue);

    struct VariableEntry *assignedVariable = NULL;
    switch (lhs->type)
    {
    case t_variable_declaration:
        assignedVariable = walkVariableDeclaration(lhs, block, scope, TACIndex, tempNum, 0);
        populateTACOperandFromVariable(&assignment->operands[0], assignedVariable);
        assignment->operands[1] = assignedValue;

        if (assignedVariable->type.basicType == vt_array)
        {
            char *arrayName = Type_GetName(&assignedVariable->type);
            LogTree(LOG_FATAL, tree, "Assignment to local array variable %s with type %s is not allowed!", assignedVariable->name, arrayName);
        }
        break;

    case t_identifier:
        assignedVariable = lookupVar(scope, lhs);
        populateTACOperandFromVariable(&assignment->operands[0], assignedVariable);
        assignment->operands[1] = assignedValue;
        break;

    // TODO: generate optimized addressing modes for arithmetic
    case t_dereference:
    {
        struct AST *writtenPointer = lhs->child;
        switch (writtenPointer->type)
        {
        case t_add:
        case t_subtract:
            walkPointerArithmetic(writtenPointer, block, scope, TACIndex, tempNum, &assignment->operands[0]);
            break;

        default:
            walkSubExpression(writtenPointer, block, scope, TACIndex, tempNum, &assignment->operands[0]);
            break;
        }
        assignment->operation = tt_store;
        assignment->operands[1] = assignedValue;
    }
    break;

    case t_array_index:
    {
        assignment->operation = tt_store;
        struct TACLine *arrayAccessLine = walkArrayRef(lhs, block, scope, TACIndex, tempNum);
        convertLoadToLea(arrayAccessLine, &assignment->operands[0]);

        assignment->operands[1] = assignedValue;
    }
    break;

    case t_dot:
    {
        assignment->operation = tt_store;
        struct TACLine *memberAccessLine = walkMemberAccess(lhs, block, scope, TACIndex, tempNum, &assignment->operands[0], 0);
        convertLoadToLea(memberAccessLine, &assignment->operands[0]);

        assignment->operands[1] = assignedValue;
    }
    break;

    default:
        LogTree(LOG_FATAL, lhs, "Unexpected AST (%s) seen in walkAssignment!", lhs->value);
        break;
    }

    if (assignment != NULL)
    {
        BasicBlock_append(block, assignment, TACIndex);
    }
}

void walkArithmeticAssignment(struct AST *tree,
                              struct BasicBlock *block,
                              struct Scope *scope,
                              size_t *TACIndex,
                              size_t *tempNum)
{
    LogTree(LOG_DEBUG, tree, "walkArithmeticAssignment");

    struct AST fakeArith = *tree;
    switch (tree->type)
    {
    case t_plus_equals:
        fakeArith.type = t_add;
        fakeArith.value = "+";
        break;

    case t_minus_equals:
        fakeArith.type = t_subtract;
        fakeArith.value = "-";
        break;

    case t_times_equals:
        fakeArith.type = t_multiply;
        fakeArith.value = "*";
        break;

    case t_divide_equals:
        fakeArith.type = t_divide;
        fakeArith.value = "/";
        break;

    case t_modulo_equals:
        fakeArith.type = t_modulo;
        fakeArith.value = "%";
        break;

    case t_bitwise_and_equals:
        fakeArith.type = t_bitwise_and;
        fakeArith.value = "&";
        break;

    case t_bitwise_or_equals:
        fakeArith.type = t_bitwise_or;
        fakeArith.value = "|";
        break;

    case t_bitwise_xor_equals:
        fakeArith.type = t_bitwise_xor;
        fakeArith.value = "^";
        break;

    case t_lshift_equals:
        fakeArith.type = t_lshift;
        fakeArith.value = "<<";
        break;

    case t_rshift_equals:
        fakeArith.type = t_rshift;
        fakeArith.value = ">>";
        break;

    default:
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkArithmeticAssignment!", getTokenName(tree->type));
    }

    // our fake arithmetic ast will have the child of the arithmetic assignment operator
    // this effectively duplicates the LHS of the assignment to the first operand of the arithmetic operator
    struct AST *lhs = tree->child;
    fakeArith.child = lhs;

    struct AST fakelhs = *lhs;
    fakelhs.sibling = &fakeArith;

    struct AST fakeAssignment = *tree;
    fakeAssignment.value = "=";
    fakeAssignment.type = t_assign;

    fakeAssignment.child = &fakelhs;

    walkAssignment(&fakeAssignment, block, scope, TACIndex, tempNum);
}

struct TACOperand *walkBitwiseNot(struct AST *tree,
                                  struct BasicBlock *block,
                                  struct Scope *scope,
                                  size_t *TACIndex,
                                  size_t *tempNum)
{
    LogTree(LOG_DEBUG, tree, "walkBitwiseNot");

    if (tree->type != t_bitwise_not)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkBitwiseNot!", getTokenName(tree->type));
    }

    // generically set to tt_add, we will actually set the operation within switch cases
    struct TACLine *bitwiseNotLine = newTACLine(tt_bitwise_not, tree);

    populateTACOperandAsTemp(&bitwiseNotLine->operands[0], tempNum);

    walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &bitwiseNotLine->operands[1]);
    copyTACOperandTypeDecayArrays(&bitwiseNotLine->operands[0], &bitwiseNotLine->operands[1]);

    struct TACOperand *operandA = &bitwiseNotLine->operands[1];

    // TODO: consistent bitwise arithmetic checking, print type name
    if ((operandA->type.pointerLevel > 0) || (operandA->type.basicType == vt_array))
    {
        LogTree(LOG_FATAL, tree, "Bitwise arithmetic on pointers is not allowed!");
    }

    BasicBlock_append(block, bitwiseNotLine, TACIndex);

    return &bitwiseNotLine->operands[0];
}

void walkSubExpression(struct AST *tree,
                       struct BasicBlock *block,
                       struct Scope *scope,
                       size_t *TACIndex,
                       size_t *tempNum,
                       struct TACOperand *destinationOperand)
{
    LogTree(LOG_DEBUG, tree, "walkSubExpression");

    switch (tree->type)
    {
        // variable read
    case t_self:
    case t_identifier:
    {
        struct VariableEntry *readVariable = lookupVar(scope, tree);
        populateTACOperandFromVariable(destinationOperand, readVariable);
    }
    break;

    case t_constant:
        destinationOperand->name.str = tree->value;
        destinationOperand->type.basicType = selectVariableTypeForLiteral(tree->value);
        destinationOperand->permutation = vp_literal;
        break;

    case t_char_literal:
    {
        size_t literalLen = strlen(tree->value);
        char literalAsNumber[sprintedNumberLength];
        if (literalLen == 1)
        {
            sprintf(literalAsNumber, "%d", tree->value[0]);
        }
        else if (literalLen == 2)
        {
            if (tree->value[0] != '\\')
            {
                LogTree(LOG_FATAL, tree, "Saw t_char_literal with escape character value of %s - expected first char to be \\!", tree->value);
            }

            char escapeCharValue = 0;

            switch (tree->value[1])
            {
            case 'a':
                escapeCharValue = '\a';
                break;

            case 'b':
                escapeCharValue = '\b';
                break;

            case 'n':
                escapeCharValue = '\n';
                break;

            case 'r':
                escapeCharValue = '\r';
                break;

            case 't':
                escapeCharValue = '\t';
                break;

            case '\\':
                escapeCharValue = '\\';
                break;

            case '\'':
                escapeCharValue = '\'';
                break;

            case '\"':
                escapeCharValue = '\"';
                break;

            default:
                LogTree(LOG_FATAL, tree, "Unexpected escape character: %s", tree->value);
            }

            sprintf(literalAsNumber, "%d", escapeCharValue);
        }
        else
        {
            LogTree(LOG_FATAL, tree, "Saw t_char_literal with string length of %lu (value '%s')!", literalLen, tree->value);
        }

        destinationOperand->name.str = Dictionary_LookupOrInsert(parseDict, literalAsNumber);
        destinationOperand->type.basicType = vt_u8;
        destinationOperand->permutation = vp_literal;
    }
    break;

    case t_string_literal:
        walkStringLiteral(tree, block, scope, destinationOperand);
        break;

    case t_function_call:
        walkFunctionCall(tree, block, scope, TACIndex, tempNum, destinationOperand);
        break;

    case t_method_call:
        walkMethodCall(tree, block, scope, TACIndex, tempNum, destinationOperand);
        break;

    case t_dot:
    {
        walkMemberAccess(tree, block, scope, TACIndex, tempNum, destinationOperand, 0);
    }
    break;

    case t_add:
    case t_subtract:
    case t_multiply:
    case t_divide:
    case t_modulo:
    case t_lshift:
    case t_rshift:
    case t_less_than:
    case t_greater_than:
    case t_less_than_equals:
    case t_greater_than_equals:
    case t_bitwise_or:
    case t_bitwise_xor:
    {
        struct TACOperand *expressionResult = walkExpression(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = *expressionResult;
    }
    break;

    case t_bitwise_not:
    {
        struct TACOperand *bitwiseNotResult = walkBitwiseNot(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = *bitwiseNotResult;
    }
    break;

    // array reference
    case t_array_index:
    {
        struct TACLine *arrayRefLine = walkArrayRef(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = arrayRefLine->operands[0];
    }
    break;

    case t_dereference:
    {
        struct TACOperand *dereferenceResult = walkDereference(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = *dereferenceResult;
    }
    break;

    case t_address_of:
    {
        struct TACOperand *addrOfResult = walkAddrOf(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = *addrOfResult;
    }
    break;

    case t_bitwise_and:
    {
        struct TACOperand *expressionResult = walkExpression(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = *expressionResult;
    }
    break;

    // TODO: helper function for casting - can better enforce validity of casting with true array types
    case t_cast:
    {
        struct TACOperand expressionResult;

        // walk the right child of the cast, the subexpression we are casting
        walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &expressionResult);
        // set the result's cast as type based on the child of the cast, the type we are casting to
        walkTypeName(tree->child, scope, &expressionResult.castAsType);

        if ((expressionResult.castAsType.basicType == vt_class) &&
            (expressionResult.castAsType.pointerLevel == 0))
        {
            char *castToType = Type_GetName(&expressionResult.castAsType);
            LogTree(LOG_FATAL, tree->child, "Casting to a class (%s) is not allowed!", castToType);
        }

        struct Type *castFrom = &expressionResult.type;
        struct Type *castTo = &expressionResult.castAsType;

        // If necessary, lop bits off the big end of the value with an explicit bitwise and operation, storing to an intermediate temp
        if (Type_CompareAllowImplicitWidening(castFrom, castTo) && (castTo->pointerLevel == 0))
        {
            struct TACLine *castBitManipulation = newTACLine(tt_bitwise_and, tree);

            // RHS of the assignment is whatever we are storing, what is being cast
            castBitManipulation->operands[1] = expressionResult;

            // construct the bit pattern we will use in order to properly mask off the extra bits (TODO: will not hold for unsigned types)
            castBitManipulation->operands[2].permutation = vp_literal;
            castBitManipulation->operands[2].type.basicType = vt_u32;

            char literalAndValue[sprintedNumberLength];
            // manually generate a string with an 'F' hex digit for each 4 bits in the mask
            sprintf(literalAndValue, "0x");
            const u8 bitsPerByte = 8; // TODO: move to substratum_defs?
            size_t maskBitWidth = (bitsPerByte * Type_GetSize(TAC_GetTypeOfOperand(castBitManipulation, 1), scope));
            size_t maskBit = 0;
            for (maskBit = 0; maskBit < maskBitWidth; maskBit += 4)
            {
                literalAndValue[2 + (maskBit / 4)] = 'F';
                literalAndValue[3 + (maskBit / 4)] = '\0';
            }

            castBitManipulation->operands[2].name.str = Dictionary_LookupOrInsert(parseDict, literalAndValue);

            // destination of our bit manipulation is a temporary variable with the type to which we are casting
            populateTACOperandAsTemp(&castBitManipulation->operands[0], tempNum);
            castBitManipulation->operands[0].type = *TAC_GetTypeOfOperand(castBitManipulation, 1);

            // attach our bit manipulation operation to the end of the basic block
            BasicBlock_append(block, castBitManipulation, TACIndex);
            // set the destination operation of this subexpression to read the manipulated value we just wrote
            *destinationOperand = castBitManipulation->operands[0];
        }
        else
        {
            // no bit manipulation required, simply set the destination operand to the result of the casted subexpression (with cast as type set by us)
            *destinationOperand = expressionResult;
        }
    }
    break;

    case t_sizeof:
        walkSizeof(tree, block, scope, destinationOperand);
        break;

    default:
        LogTree(LOG_FATAL, tree, "Incorrect AST type (%s) seen while linearizing subexpression!", getTokenName(tree->type));
        break;
    }
}

void checkFunctionReturnUse(struct AST *tree,
                            struct TACOperand *destinationOperand,
                            struct FunctionEntry *calledFunction)
{
    if ((destinationOperand != NULL) &&
        (calledFunction->returnType.basicType == vt_null))
    {
        LogTree(LOG_FATAL, tree, "Attempt to use return value of function %s which does not return anything!", calledFunction->name);
    }
}

struct Stack *walkArgumentPushes(struct AST *argumentRunner,
                                 struct FunctionEntry *calledFunction,
                                 struct BasicBlock *block,
                                 struct Scope *scope,
                                 size_t *TACIndex,
                                 size_t *tempNum,
                                 u8 forMethod) // if walking argument pushes for method, adjust indexing to skip the "self" parameter
{
    Log(LOG_DEBUG, "walkArgumentPushes");

    u8 argumentNumOffset = 0;
    if (forMethod)
    {
        argumentNumOffset = 1;
    }

    // save first argument so we can generate meaningful error messages if we mismatch argument count
    struct AST *lastArgument = argumentRunner;

    struct Stack *argumentTrees = Stack_New();
    while (argumentRunner != NULL)
    {
        Stack_Push(argumentTrees, argumentRunner);
        lastArgument = argumentRunner;
        argumentRunner = argumentRunner->sibling;
    }

    struct Stack *argumentPushes = Stack_New();
    if (argumentTrees->size != (calledFunction->arguments->size - argumentNumOffset))
    {
        LogTree(LOG_FATAL, lastArgument,
                "Error in call to function %s - expected %zu arguments, saw %zu!",
                calledFunction->name,
                calledFunction->arguments->size,
                argumentTrees->size);
    }

    size_t argIndex = calledFunction->arguments->size - 1;
    while (argumentTrees->size > 0)
    {
        struct AST *pushedArgument = Stack_Pop(argumentTrees);
        struct TACLine *push = newTACLine(tt_stack_store, pushedArgument);
        Stack_Push(argumentPushes, push);
        walkSubExpression(pushedArgument, block, scope, TACIndex, tempNum, &push->operands[0]);

        struct VariableEntry *expectedArgument = calledFunction->arguments->data[argIndex];

        if (Type_CompareAllowImplicitWidening(TAC_GetTypeOfOperand(push, 0), &expectedArgument->type))
        {
            LogTree(LOG_FATAL, pushedArgument,
                    "Error in argument %s passed to function %s!\n\tExpected %s, got %s",
                    expectedArgument->name,
                    calledFunction->name,
                    Type_GetName(&expectedArgument->type),
                    Type_GetName(TAC_GetTypeOfOperand(push, 0)));
        }

        struct TACOperand decayed;
        copyTACOperandDecayArrays(&decayed, &push->operands[0]);

        // allow us to automatically widen
        if (Type_GetSize(TACOperand_GetType(&decayed), scope) <= Type_GetSize(&expectedArgument->type, scope))
        {
            push->operands[0].castAsType = expectedArgument->type;
        }
        else
        {
            char *convertFromType = Type_GetName(&push->operands[0].type);
            char *convertToType = Type_GetName(&expectedArgument->type);
            LogTree(LOG_FATAL, pushedArgument,
                    "Potential narrowing conversion passed to argument %s of function %s\n\tConversion from %s to %s",
                    expectedArgument->name,
                    calledFunction->name,
                    convertFromType,
                    convertToType);
        }

        push->operands[1].name.val = expectedArgument->stackOffset;
        push->operands[1].type.basicType = vt_u64;
        push->operands[1].permutation = vp_literal;

        argIndex--;
    }
    Stack_Free(argumentTrees);

    return argumentPushes;
}

void reserveAndStoreStackArgs(struct AST *callTree, struct FunctionEntry *calledFunction, struct Stack *argumentPushes, struct BasicBlock *block, size_t *TACIndex)
{
    LogTree(LOG_DEBUG, callTree, "reserveAndStoreStackArgs");

    if (calledFunction->arguments->size > 0)
    {
        struct TACLine *reserveStackSpaceForCall = newTACLine(tt_stack_reserve, callTree);
        if (calledFunction->argStackSize > I64_MAX)
        {
            // TODO: implementation dependent size of size_t
            InternalError("Function %s has arg stack size too large (%zd bytes)!", calledFunction->name, calledFunction->argStackSize);
        }
        reserveStackSpaceForCall->operands[0].name.val = (ssize_t)calledFunction->argStackSize;
        BasicBlock_append(block, reserveStackSpaceForCall, TACIndex);
    }

    while (argumentPushes->size > 0)
    {
        struct TACLine *push = Stack_Pop(argumentPushes);
        BasicBlock_append(block, push, TACIndex);
    }
}

struct TACLine *generateCallTac(struct AST *callTree,
                                struct FunctionEntry *calledFunction,
                                struct BasicBlock *block,
                                size_t *TACIndex,
                                size_t *tempNum,
                                struct TACOperand *destinationOperand)
{
    LogTree(LOG_DEBUG, callTree, "generateCallTac");

    struct TACLine *call = newTACLine(tt_function_call, callTree);
    call->operands[1].name.str = calledFunction->name;
    BasicBlock_append(block, call, TACIndex);

    if (destinationOperand != NULL)
    {
        call->operands[0].type = calledFunction->returnType;
        populateTACOperandAsTemp(&call->operands[0], tempNum);

        *destinationOperand = call->operands[0];
    }

    return call;
}

void walkFunctionCall(struct AST *tree,
                      struct BasicBlock *block,
                      struct Scope *scope,
                      size_t *TACIndex,
                      size_t *tempNum,
                      struct TACOperand *destinationOperand)
{
    LogTree(LOG_DEBUG, tree, "walkFunctionCall");

    if (tree->type != t_function_call)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkFunctionCall!", getTokenName(tree->type));
    }

    scope->parentFunction->callsOtherFunction = 1;

    struct FunctionEntry *calledFunction = lookupFun(scope, tree->child);

    checkFunctionReturnUse(tree, destinationOperand, calledFunction);

    struct Stack *argumentPushes = walkArgumentPushes(tree->child->sibling,
                                                      calledFunction,
                                                      block,
                                                      scope,
                                                      TACIndex,
                                                      tempNum,
                                                      0);

    reserveAndStoreStackArgs(tree, calledFunction, argumentPushes, block, TACIndex);

    Stack_Free(argumentPushes);

    generateCallTac(tree, calledFunction, block, TACIndex, tempNum, destinationOperand);
}

struct TACOperand *getAddrOfOperand(struct AST *tree,
                                    struct BasicBlock *block,
                                    struct Scope *scope,
                                    size_t *TACIndex,
                                    size_t *tempNum,
                                    struct TACOperand *getAddrOf)
{
    struct TACLine *addrOfLine = newTACLine(tt_addrof, tree);
    addrOfLine->operands[1] = *getAddrOf;

    populateTACOperandAsTemp(&addrOfLine->operands[0], tempNum);

    copyTACOperandTypeDecayArrays(&addrOfLine->operands[0], &addrOfLine->operands[1]);
    TAC_GetTypeOfOperand(addrOfLine, 0)->pointerLevel++;
    BasicBlock_append(block, addrOfLine, TACIndex);

    return &addrOfLine->operands[0];
}

void walkMethodCall(struct AST *tree,
                    struct BasicBlock *block,
                    struct Scope *scope,
                    size_t *TACIndex,
                    size_t *tempNum,
                    struct TACOperand *destinationOperand)
{
    LogTree(LOG_DEBUG, tree, "walkMethodCall");

    if (tree->type != t_method_call)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkMethodCall!", getTokenName(tree->type));
    }

    // don't need to track scope->parentFunction->callsOtherFunction as walkFunctionCall will do this on our behalf
    struct AST *classTree = tree->child->child;
    struct ClassEntry *classCalledOn = NULL;
    struct AST *callTree = tree->child->child->sibling;

    struct TACOperand classOperand;
    memset(&classOperand, 0, sizeof(struct TACOperand));

    if (classTree == t_identifier)
    {
        classCalledOn = lookupClass(scope, classTree);
    }
    else
    {
        walkSubExpression(classTree, block, scope, TACIndex, tempNum, &classOperand);
        if (TACOperand_GetType(&classOperand)->basicType != vt_class)
        {
            char *nonClassType = Type_GetName(TACOperand_GetType(&classOperand));
            LogTree(LOG_FATAL, classTree, "Attempt to call method %s on non-class type %s", callTree->child->value, nonClassType);
        }
        classCalledOn = lookupClassByType(scope, TACOperand_GetType(&classOperand));

        // TODO: check arrow vs dot operator against indirection level here?
    }

    struct FunctionEntry *calledFunction = lookupMethod(classCalledOn, callTree->child);

    checkFunctionReturnUse(tree, destinationOperand, calledFunction);

    struct Stack *argumentPushes = walkArgumentPushes(tree->child->child->sibling->child->sibling,
                                                      calledFunction,
                                                      block,
                                                      scope,
                                                      TACIndex,
                                                      tempNum,
                                                      1);

    if (TACOperand_GetType(&classOperand)->basicType == vt_array)
    {
        char *nonDottableType = Type_GetName(TACOperand_GetType(&classOperand));
        LogTree(LOG_FATAL, callTree, "Attempt to call method %s on non-dottable type %s", calledFunction->name, nonDottableType);
    }

    // if class we are calling method on is not indirect, automagically insert an intermediate address-of
    if (TACOperand_GetType(&classOperand)->pointerLevel == 0)
    {
        classOperand = *getAddrOfOperand(tree, block, scope, TACIndex, tempNum, &classOperand);
    }

    struct TACLine *pThisPush = newTACLine(tt_stack_store, classTree);
    pThisPush->operands[0] = classOperand;
    pThisPush->operands[1].name.val = 0;
    pThisPush->operands[1].type.basicType = vt_u64;
    pThisPush->operands[1].permutation = vp_literal;

    Stack_Push(argumentPushes, pThisPush);

    reserveAndStoreStackArgs(tree, calledFunction, argumentPushes, block, TACIndex);

    Stack_Free(argumentPushes);

    struct TACLine *callLine = generateCallTac(tree, calledFunction, block, TACIndex, tempNum, destinationOperand);
    callLine->operation = tt_method_call;
    callLine->operands[2].type.basicType = vt_class;
    callLine->operands[2].type.nonArray.complexType.name = classCalledOn->name;
}

struct TACLine *walkMemberAccess(struct AST *tree,
                                 struct BasicBlock *block,
                                 struct Scope *scope,
                                 size_t *TACIndex,
                                 size_t *tempNum,
                                 struct TACOperand *srcDestOperand,
                                 size_t depth)
{
    LogTree(LOG_DEBUG, tree, "walkMemberAccess");

    if (tree->type != t_dot)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkMemberAccess!", getTokenName(tree->type));
    }

    struct AST *lhs = tree->child;
    struct AST *rhs = lhs->sibling;

    if (rhs->type != t_identifier)
    {
        LogTree(LOG_FATAL, rhs,
                "Expected identifier on RHS of %s operator, got %s (%s) instead!",
                getTokenName(tree->type),
                rhs->value,
                getTokenName(rhs->type));
    }

    struct TACLine *accessLine = NULL;

    switch (lhs->type)
    {
    // in the case that we are dot-ing a LHS which is a dot, walkMemberAccess will generate the TAC line for the *read* which gets us the address to dot on
    case t_dot:
        accessLine = walkMemberAccess(lhs, block, scope, TACIndex, tempNum, srcDestOperand, depth + 1);
        break;

    // for all other cases, we can  populate the LHS using walkSubExpression as it is just a more basic read
    default:
    {
        // the LHS of the dot is the class instance being accessed
        struct AST *class = tree->child;
        // the RHS is what member we are accessing
        struct AST *member = tree->child->sibling;

        // TODO: check more deeply what's being dotted? Shortlist: dereference, array index, identifier, maybe some pointer arithmetic?
        //		 	- things like (myObjectPointer & 0xFFFFFFF0)->member are obviously wrong, so probably should disallow
        // prevent silly things like (&a)->b

        if (member->type != t_identifier)
        {
            LogTree(LOG_FATAL, member,
                    "Expected identifier on RHS of dot operator, got %s (%s) instead!",
                    member->value,
                    getTokenName(member->type));
        }

        // our access line is a completely new TAC line, which is a load operation with an offset, storing the load result to a temp
        accessLine = newTACLine(tt_load_off, tree);

        populateTACOperandAsTemp(&accessLine->operands[0], tempNum);

        // we may need to do some manipulation of the subexpression depending on what exactly we're dotting
        switch (class->type)
        {
        case t_dereference:
        {
            // let walkDereference do the heavy lifting for us
            struct TACOperand *dereferencedOperand = walkDereference(class, block, scope, TACIndex, tempNum);

            // make sure we are generally dotting something sane
            struct Type *accessedType = TACOperand_GetType(dereferencedOperand);

            checkAccessedClassForDot(class, scope, accessedType);
            // additional check so that if we dereference a class single-pointer we force not putting the dereference there
            if (accessedType->pointerLevel == 0)
            {
                char *dereferencedTypeName = Type_GetName(accessedType);
                LogTree(LOG_FATAL, class, "Use of dereference on single-indirect type %s before dot '(*class).member' is prohibited - just use 'class.member' instead", dereferencedTypeName);
            }

            copyTACOperandDecayArrays(&accessLine->operands[1], dereferencedOperand);
        }
        break;

        case t_array_index:
        {
            // let walkArrayRef do the heavy lifting for us
            struct TACLine *arrayRefToDot = walkArrayRef(class, block, scope, TACIndex, tempNum);

            // before we convert our array ref to an LEA to get the address of the class we're dotting, check to make sure everything is good
            checkAccessedClassForDot(tree, scope, TAC_GetTypeOfOperand(arrayRefToDot, 0));

            // now that we know we are dotting something valid, we will just use the array reference as an address calculation for the base of whatever we're dotting
            convertLoadToLea(arrayRefToDot, &accessLine->operands[1]);
        }
        break;

        case t_self:
        case t_identifier:
        {
            // if we are dotting an identifier, insert an address-of if it is not a pointer already
            struct VariableEntry *dottedVariable = lookupVar(scope, class);

            if (dottedVariable->type.pointerLevel == 0)
            {
                struct TACOperand dottedOperand;
                memset(&dottedOperand, 0, sizeof(struct TACOperand));

                walkSubExpression(class, block, scope, TACIndex, tempNum, &dottedOperand);

                if (dottedOperand.permutation != vp_temp)
                {
                    // while this check is duplicated in the checks immediately following the switch,
                    // we may be able to print more verbose error info if we are directly member-accessing an identifier, so do it here.
                    checkAccessedClassForDot(class, scope, TACOperand_GetType(&dottedOperand));
                }

                struct TACOperand *addrOfDottedVariable = getAddrOfOperand(class, block, scope, TACIndex, tempNum, &dottedOperand);
                copyTACOperandDecayArrays(&accessLine->operands[1], addrOfDottedVariable);
            }
            else
            {
                walkSubExpression(class, block, scope, TACIndex, tempNum, &accessLine->operands[1]);
            }
        }
        break;

        default:
            LogTree(LOG_FATAL, class, "Dot operator member access on disallowed tree type %s", getTokenName(class->type));
            break;
        }

        accessLine->operands[2].type.basicType = vt_u32;
        accessLine->operands[2].permutation = vp_literal;
    }
    break;
    }

    struct Type *accessedType = TAC_GetTypeOfOperand(accessLine, 1);
    if (accessedType->basicType != vt_class)
    {
        char *accessedTypeName = Type_GetName(accessedType);
        LogTree(LOG_FATAL, tree, "Use of dot operator for member access on non-class type %s", accessedTypeName);
    }

    // get the ClassEntry and ClassMemberOffset of what we're accessing within and the member we access
    struct ClassEntry *accessedClass = lookupClassByType(scope, accessedType);
    struct ClassMemberOffset *accessedMember = lookupMemberVariable(accessedClass, rhs);

    // populate type information (use cast for the first operand as we are treating a class as a pointer to something else with a given offset)
    accessLine->operands[1].castAsType = accessedMember->variable->type;
    accessLine->operands[0].type = *TAC_GetTypeOfOperand(accessLine, 1);               // copy type info to the temp we're reading to
    copyTACOperandTypeDecayArrays(&accessLine->operands[0], &accessLine->operands[0]); // decay arrays in-place so we only have pointers instead of arrays

    accessLine->operands[2].name.val += accessedMember->offset;

    if (depth == 0)
    {
        BasicBlock_append(block, accessLine, TACIndex);
        *srcDestOperand = accessLine->operands[0];
    }

    return accessLine;
}

void walkNonPointerArithmetic(struct AST *tree,
                              struct BasicBlock *block,
                              struct Scope *scope,
                              size_t *TACIndex,
                              size_t *tempNum,
                              struct TACLine *expression)
{
    LogTree(LOG_DEBUG, tree, "walkNonPointerArithmetic");

    switch (tree->type)
    {
    case t_multiply:
        expression->reorderable = 1;
        expression->operation = tt_mul;
        break;

    case t_bitwise_and:
        expression->reorderable = 1;
        expression->operation = tt_bitwise_and;
        break;

    case t_bitwise_or:
        expression->reorderable = 1;
        expression->operation = tt_bitwise_or;
        break;

    case t_bitwise_xor:
        expression->reorderable = 1;
        expression->operation = tt_bitwise_xor;
        break;

    case t_bitwise_not:
        expression->reorderable = 1;
        expression->operation = tt_bitwise_not;
        break;

    case t_divide:
        expression->operation = tt_div;
        break;

    case t_modulo:
        expression->operation = tt_modulo;
        break;

    case t_lshift:
        expression->operation = tt_lshift;
        break;

    case t_rshift:
        expression->operation = tt_rshift;
        break;

    default:
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkNonPointerArithmetic!", getTokenName(tree->type));
        break;
    }

    walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &expression->operands[1]);
    walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &expression->operands[2]);

    for (u8 operandIndex = 1; operandIndex < 2; operandIndex++)
    {
        struct Type *checkedType = TAC_GetTypeOfOperand(expression, operandIndex);
        if ((checkedType->pointerLevel > 0) || (checkedType->basicType == vt_array))
        {
            char *typeName = Type_GetName(checkedType);
            LogTree(LOG_FATAL, tree->child, "Arithmetic operation attempted on type %s, %s is only allowed on non-indirect types", typeName, tree->value);
        }
    }

    if (Type_GetSize(TAC_GetTypeOfOperand(expression, 1), scope) > Type_GetSize(TAC_GetTypeOfOperand(expression, 2), scope))
    {
        copyTACOperandTypeDecayArrays(&expression->operands[0], &expression->operands[1]);
    }
    else
    {
        copyTACOperandTypeDecayArrays(&expression->operands[0], &expression->operands[2]);
    }
}

struct TACOperand *walkExpression(struct AST *tree,
                                  struct BasicBlock *block,
                                  struct Scope *scope,
                                  size_t *TACIndex,
                                  size_t *tempNum)
{
    LogTree(LOG_DEBUG, tree, "walkExpression");

    // generically set to tt_add, we will actually set the operation within switch cases
    struct TACLine *expression = newTACLine(tt_subtract, tree);

    populateTACOperandAsTemp(&expression->operands[0], tempNum);

    u8 fallingThrough = 0;

    switch (tree->type)
    {
    // basic arithmetic
    case t_multiply:
    case t_bitwise_and:
    case t_bitwise_or:
    case t_bitwise_xor:
    case t_bitwise_not:
    case t_divide:
    case t_modulo:
    case t_lshift:
    case t_rshift:
        walkNonPointerArithmetic(tree, block, scope, TACIndex, tempNum, expression);
        break;

    case t_add:
        expression->operation = tt_add;
        expression->reorderable = 1;
        fallingThrough = 1;
    case t_subtract:
    {
        if (!fallingThrough)
        {
            expression->operation = tt_subtract;
            fallingThrough = 1;
        }

        walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &expression->operands[1]);

        // TODO: also scale arithmetic on array types
        if (TAC_GetTypeOfOperand(expression, 1)->pointerLevel > 0)
        {
            struct TACLine *scaleMultiply = setUpScaleMultiplication(tree, scope, TACIndex, tempNum, TAC_GetTypeOfOperand(expression, 1));
            walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &scaleMultiply->operands[1]);

            scaleMultiply->operands[0].type = scaleMultiply->operands[1].type;
            copyTACOperandDecayArrays(&expression->operands[2], &scaleMultiply->operands[0]);

            BasicBlock_append(block, scaleMultiply, TACIndex);
        }
        else
        {
            walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &expression->operands[2]);
        }

        // TODO: generate errors for array types
        struct TACOperand *operandA = &expression->operands[1];
        struct TACOperand *operandB = &expression->operands[2];
        if ((operandA->type.pointerLevel > 0) && (operandB->type.pointerLevel > 0))
        {
            LogTree(LOG_FATAL, tree, "Arithmetic between 2 pointers is not allowed!");
        }

        // TODO generate errors for bad pointer arithmetic here
        if (Type_GetSize(TACOperand_GetType(operandA), scope) > Type_GetSize(TACOperand_GetType(operandB), scope))
        {
            copyTACOperandTypeDecayArrays(&expression->operands[0], operandA);
        }
        else
        {
            copyTACOperandTypeDecayArrays(&expression->operands[0], operandB);
        }
    }
    break;

    default:
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkExpression!", getTokenName(tree->type));
    }

    BasicBlock_append(block, expression, TACIndex);

    return &expression->operands[0];
}

struct TACLine *walkArrayRef(struct AST *tree,
                             struct BasicBlock *block,
                             struct Scope *scope,
                             size_t *TACIndex,
                             size_t *tempNum)
{
    LogTree(LOG_DEBUG, tree, "walkArrayRef");

    if (tree->type != t_array_index)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkArrayRef!", getTokenName(tree->type));
    }

    struct AST *arrayBase = tree->child;
    struct AST *arrayIndex = tree->child->sibling;

    struct TACLine *arrayRefTAC = newTACLine(tt_load_arr, tree);
    struct Type *arrayBaseType = NULL;

    switch (arrayBase->type)
    {
    // if the array base is an identifier, we can just look it up
    case t_identifier:
    {
        struct VariableEntry *arrayVariable = lookupVar(scope, arrayBase);
        populateTACOperandFromVariable(&arrayRefTAC->operands[1], arrayVariable);
        arrayBaseType = TAC_GetTypeOfOperand(arrayRefTAC, 1);

        // sanity check - can print the name of the variable if incorrectly accessing an identifier
        // TODO: check against size of array if index is constant?
        if ((arrayBaseType->pointerLevel == 0) && (arrayBaseType->basicType != vt_array))
        {
            LogTree(LOG_FATAL, arrayBase, "Array reference on non-indirect variable %s %s", Type_GetName(arrayBaseType), arrayBase->value);
        }
    }
    break;

    case t_dot:
    {
        struct TACLine *arrayBaseAccessLine = walkMemberAccess(arrayBase, block, scope, TACIndex, tempNum, &arrayRefTAC->operands[1], 0);
        convertLoadToLea(arrayBaseAccessLine, &arrayRefTAC->operands[1]);
        arrayBaseType = TAC_GetTypeOfOperand(arrayBaseAccessLine, 0);
    }
    break;

    // otherwise, we need to walk the subexpression to get the array base
    default:
    {
        walkSubExpression(arrayBase, block, scope, TACIndex, tempNum, &arrayRefTAC->operands[1]);
        arrayBaseType = TAC_GetTypeOfOperand(arrayRefTAC, 1);

        // sanity check - can only print the type of the base if incorrectly accessing a non-identifier through a subexpression
        if ((arrayBaseType->pointerLevel == 0) && (arrayBaseType->basicType != vt_array))
        {
            LogTree(LOG_FATAL, arrayBase, "Array reference on non-indirect type %s", Type_GetName(arrayBaseType));
        }
    }
    break;
    }

    copyTACOperandDecayArrays(&arrayRefTAC->operands[0], &arrayRefTAC->operands[1]);
    populateTACOperandAsTemp(&arrayRefTAC->operands[0], tempNum);
    arrayRefTAC->operands[0].type.pointerLevel--;

    if (arrayIndex->type == t_constant)
    {
        // if referencing an array of classes, implicitly convert to an LEA to avoid copying the entire class to a temp
        if ((arrayBaseType->basicType == vt_class) && (arrayBaseType->pointerLevel == 0))
        {
            arrayRefTAC->operation = tt_lea_off;
            arrayRefTAC->operands[0].type.pointerLevel++;
        }
        else
        {
            arrayRefTAC->operation = tt_load_off;
        }

        // TODO: abstract this
        int indexSize = atoi(arrayIndex->value);
        indexSize *= 1 << alignSize(Type_GetSizeOfArrayElement(arrayBaseType, scope));

        arrayRefTAC->operands[2].name.val = indexSize;
        arrayRefTAC->operands[2].permutation = vp_literal;
        arrayRefTAC->operands[2].type.basicType = selectVariableTypeForNumber(arrayRefTAC->operands[2].name.val);
    }
    // otherwise, the index is either a variable or subexpression
    else
    {
        // if referencing an array of classes, implicitly convert to an LEA to avoid copying the entire class to a temp
        if ((arrayBaseType->basicType == vt_class) && (arrayBaseType->pointerLevel == 0))
        {
            arrayRefTAC->operation = tt_lea_arr;
            arrayRefTAC->operands[0].type.pointerLevel++;
        }
        // set the scale for the array access

        arrayRefTAC->operands[3].name.val = alignSize(Type_GetSizeOfArrayElement(arrayBaseType, scope));
        arrayRefTAC->operands[3].permutation = vp_literal;
        arrayRefTAC->operands[3].type.basicType = selectVariableTypeForNumber(arrayRefTAC->operands[3].name.val);

        walkSubExpression(arrayIndex, block, scope, TACIndex, tempNum, &arrayRefTAC->operands[2]);
    }

    BasicBlock_append(block, arrayRefTAC, TACIndex);
    return arrayRefTAC;
}

struct TACOperand *walkDereference(struct AST *tree,
                                   struct BasicBlock *block,
                                   struct Scope *scope,
                                   size_t *TACIndex,
                                   size_t *tempNum)
{
    LogTree(LOG_DEBUG, tree, "walkDereference");

    if (tree->type != t_dereference)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkDereference!", getTokenName(tree->type));
    }

    struct TACLine *dereference = newTACLine(tt_load, tree);

    switch (tree->child->type)
    {
    case t_add:
    case t_subtract:
    {
        walkPointerArithmetic(tree->child, block, scope, TACIndex, tempNum, &dereference->operands[1]);
    }
    break;

    default:
        walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &dereference->operands[1]);
        break;
    }

    copyTACOperandDecayArrays(&dereference->operands[0], &dereference->operands[1]);
    TAC_GetTypeOfOperand(dereference, 0)->pointerLevel--;
    populateTACOperandAsTemp(&dereference->operands[0], tempNum);

    BasicBlock_append(block, dereference, TACIndex);

    return &dereference->operands[0];
}

struct TACOperand *walkAddrOf(struct AST *tree,
                              struct BasicBlock *block,
                              struct Scope *scope,
                              size_t *TACIndex,
                              size_t *tempNum)
{
    LogTree(LOG_DEBUG, tree, "walkAddrOf");

    if (tree->type != t_address_of)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkAddressOf!", getTokenName(tree->type));
    }

    // TODO: helper function for getting address of
    struct TACLine *addrOfLine = newTACLine(tt_addrof, tree);
    populateTACOperandAsTemp(&addrOfLine->operands[0], tempNum);

    switch (tree->child->type)
    {
    // look up the variable entry and ensure that we will spill it to the stack since we take its address
    case t_identifier:
    {
        struct VariableEntry *addrTakenOf = lookupVar(scope, tree->child);
        if (addrTakenOf->type.basicType == vt_array)
        {
            LogTree(LOG_FATAL, tree->child, "Can't take address of local array %s!", addrTakenOf->name);
        }
        addrTakenOf->mustSpill = 1;
        walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &addrOfLine->operands[1]);
    }
    break;

    case t_array_index:
    {
        // use walkArrayRef to generate the access we need, just the direct accessing load to an lea to calculate the address we would have loaded from
        struct TACLine *arrayRefLine = walkArrayRef(tree->child, block, scope, TACIndex, tempNum);
        convertLoadToLea(arrayRefLine, NULL);

        // early return, no need for explicit address-of TAC
        freeTAC(addrOfLine);
        addrOfLine = NULL;

        return &arrayRefLine->operands[0];
    }
    break;

    case t_dot:
    {
        // walkMemberAccess can do everything we need
        // the only thing we have to do is ensure we have an LEA at the end instead of a direct read in the case of the dot operator
        struct TACLine *memberAccessLine = walkMemberAccess(tree->child, block, scope, TACIndex, tempNum, &addrOfLine->operands[1], 0);
        convertLoadToLea(memberAccessLine, &addrOfLine->operands[1]);

        // free the line created at the top of this function and return early
        freeTAC(addrOfLine);
        return &memberAccessLine->operands[0];
    }
    break;

    default:
        LogTree(LOG_FATAL, tree, "Address of operator is not supported for non-identifiers! Saw %s", getTokenName(tree->child->type));
    }

    addrOfLine->operands[0].type = *TAC_GetTypeOfOperand(addrOfLine, 1);
    addrOfLine->operands[0].type.pointerLevel++;

    BasicBlock_append(block, addrOfLine, TACIndex);

    return &addrOfLine->operands[0];
}

void walkPointerArithmetic(struct AST *tree,
                           struct BasicBlock *block,
                           struct Scope *scope,
                           size_t *TACIndex,
                           size_t *tempNum,
                           struct TACOperand *destinationOperand)
{
    LogTree(LOG_DEBUG, tree, "walkPointerArithmetic");

    if ((tree->type != t_add) && (tree->type != t_subtract))
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkPointerArithmetic!", getTokenName(tree->type));
    }

    struct AST *pointerArithLHS = tree->child;
    struct AST *pointerArithRHS = tree->child->sibling;

    struct TACLine *pointerArithmetic = newTACLine(tt_add, tree->child);
    if (tree->type == t_subtract)
    {
        pointerArithmetic->operation = tt_subtract;
    }

    walkSubExpression(pointerArithLHS, block, scope, TACIndex, tempNum, &pointerArithmetic->operands[1]);

    populateTACOperandAsTemp(&pointerArithmetic->operands[0], tempNum);
    copyTACOperandDecayArrays(&pointerArithmetic->operands[0], &pointerArithmetic->operands[1]);

    struct TACLine *scaleMultiplication = setUpScaleMultiplication(pointerArithRHS,
                                                                   scope,
                                                                   TACIndex,
                                                                   tempNum,
                                                                   TAC_GetTypeOfOperand(pointerArithmetic, 1));

    walkSubExpression(pointerArithRHS, block, scope, TACIndex, tempNum, &scaleMultiplication->operands[1]);

    copyTACOperandTypeDecayArrays(&scaleMultiplication->operands[0], &scaleMultiplication->operands[1]);

    copyTACOperandDecayArrays(&pointerArithmetic->operands[2], &scaleMultiplication->operands[0]);

    BasicBlock_append(block, scaleMultiplication, TACIndex);
    BasicBlock_append(block, pointerArithmetic, TACIndex);

    *destinationOperand = pointerArithmetic->operands[0];
}

void walkAsmBlock(struct AST *tree,
                  struct BasicBlock *block,
                  struct Scope *scope,
                  size_t *TACIndex,
                  size_t *tempNum)
{
    LogTree(LOG_DEBUG, tree, "walkAsmBlock");

    if (tree->type != t_asm)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkAsmBlock!", getTokenName(tree->type));
    }

    struct AST *asmRunner = tree->child;
    while (asmRunner != NULL)
    {
        if (asmRunner->type != t_asm)
        {
            LogTree(LOG_FATAL, tree, "Non-asm seen as contents of ASM block!");
        }

        struct TACLine *asmLine = newTACLine(tt_asm, asmRunner);
        asmLine->operands[0].name.str = asmRunner->value;

        BasicBlock_append(block, asmLine, TACIndex);

        asmRunner = asmRunner->sibling;
    }
}

void walkStringLiteral(struct AST *tree,
                       struct BasicBlock *block,
                       struct Scope *scope,
                       struct TACOperand *destinationOperand)
{
    LogTree(LOG_DEBUG, tree, "walkStringLiteral");

    if (tree->type != t_string_literal)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkStringLiteral!", getTokenName(tree->type));
    }

    // it inserts underscores in place of spaces and other modifications to turn the literal into a name that the symtab can use
    // but first, it copies the string exactly as-is so it knows what the string object should be initialized to
    char *stringName = tree->value;
    char *stringValue = strdup(stringName);
    size_t stringLength = strlen(stringName);

    for (size_t charIndex = 0; charIndex < stringLength; charIndex++)
    {
        if ((!isalnum(stringName[charIndex])) && (stringName[charIndex] != '_'))
        {
            if (isspace(stringName[charIndex]))
            {
                stringName[charIndex] = '_';
            }
            else
            {
                // for any non-whitespace character, map it to lower/uppercase alphabetic characters
                // this should avoid collisions with renamed strings to the point that it isn't a problem
                const u16 charsInAlphabet = 26;
                char altVal = (char)(stringName[charIndex] % (charsInAlphabet * 1));
                if (altVal > (charsInAlphabet - 1))
                {
                    stringName[charIndex] = (char)(altVal + 'A');
                }
                else
                {
                    stringName[charIndex] = (char)(altVal + 'a');
                }
            }
        }
    }

    struct VariableEntry *stringLiteralEntry = NULL;
    struct ScopeMember *existingMember = Scope_lookup(scope, stringName);

    // if we already have a string literal for this thing, nothing else to do
    if (existingMember == NULL)
    {
        struct AST fakeStringTree;
        fakeStringTree.value = stringName;
        fakeStringTree.sourceFile = tree->sourceFile;
        fakeStringTree.sourceLine = tree->sourceLine;
        fakeStringTree.sourceCol = tree->sourceCol;

        struct Type stringType;
        Type_SetBasicType(&stringType, vt_array, NULL, 0);
        struct Type charType;
        Type_Init(&charType);
        charType.basicType = vt_u8;
        stringType.array.type = Dictionary_LookupOrInsert(typeDict, &charType);
        stringType.array.size = stringLength;

        stringLiteralEntry = createVariable(scope, &fakeStringTree, &stringType, 1, 0, 0);
        stringLiteralEntry->isStringLiteral = 1;

        struct Type *realStringType = &stringLiteralEntry->type;
        realStringType->array.initializeArrayTo = malloc(stringLength * sizeof(char *));
        for (size_t charIndex = 0; charIndex < stringLength; charIndex++)
        {
            realStringType->array.initializeArrayTo[charIndex] = malloc(sizeof(char));
            *(char *)realStringType->array.initializeArrayTo[charIndex] = stringValue[charIndex];
        }
    }
    else
    {
        stringLiteralEntry = existingMember->entry;
    }

    free(stringValue);
    populateTACOperandFromVariable(destinationOperand, stringLiteralEntry);
    destinationOperand->name.str = stringName;
    destinationOperand->type = stringLiteralEntry->type;
}

void walkSizeof(struct AST *tree,
                struct BasicBlock *block,
                struct Scope *scope,
                struct TACOperand *destinationOperand)
{
    LogTree(LOG_DEBUG, tree, "walkSizeof");

    if (tree->type != t_sizeof)
    {
        LogTree(LOG_FATAL, tree, "Wrong AST (%s) passed to walkSizeof!", getTokenName(tree->type));
    }

    size_t sizeInBytes = 0;

    switch (tree->child->type)
    {
    // if we see an identifier, it may be an identifier or a class name
    case t_identifier:
    {
        // do a generic scope lookup on the identifier
        struct ScopeMember *lookedUpIdentifier = Scope_lookup(scope, tree->child->value);

        // if it looks up nothing, or it's a variable
        if ((lookedUpIdentifier == NULL) || (lookedUpIdentifier->type == e_variable))
        {
            // Scope_lookupVar is not redundant as it will give us a 'use of undeclared' error in the case where we looked up nothing
            struct VariableEntry *getSizeof = lookupVar(scope, tree->child);

            sizeInBytes = Type_GetSize(&getSizeof->type, scope);
        }
        // we looked something up but it's not a variable
        else
        {
            struct ClassEntry *getSizeof = lookupClass(scope, tree->child);

            sizeInBytes = getSizeof->totalSize;
        }
    }
    break;

    case t_type_name:
    {
        struct Type getSizeof;
        walkTypeName(tree->child, scope, &getSizeof);

        sizeInBytes = Type_GetSize(&getSizeof, scope);
    }
    break;
    default:
        LogTree(LOG_FATAL, tree, "sizeof is only supported on type names and identifiers!");
    }

    char sizeString[sprintedNumberLength];
    snprintf(sizeString, sprintedNumberLength - 1, "%zu", sizeInBytes);
    destinationOperand->type.basicType = vt_u8;
    destinationOperand->permutation = vp_literal;
    destinationOperand->name.str = Dictionary_LookupOrInsert(parseDict, sizeString);
}