#include "linearizer.h"
#include "codegen_generic.h"
#include "linearizer_generic.h"

#include "log.h"
#include "symtab.h"

#include <ctype.h>

#define OUT_OBJECT_POINTER_NAME "__out_obj_pointer"

/*
 * These functions Walk the AST and convert it to three-address code
 */
struct TempList *temps;
struct Dictionary *typeDict;
extern struct Dictionary *parseDict;
const u8 TYPE_DICT_SIZE = 10;
struct SymbolTable *walk_program(struct AST *program)
{
    typeDict = dictionary_new(TYPE_DICT_SIZE, (void *(*)(void *))type_duplicate, (size_t(*)(void *))type_hash, (ssize_t(*)(void *, void *))type_compare, (void (*)(void *))type_free);
    struct SymbolTable *programTable = symbol_table_new("Program");
    struct BasicBlock *globalBlock = scope_lookup(programTable->globalScope, "globalblock")->entry;
    struct BasicBlock *asmBlock = basic_block_new(1);
    scope_add_basic_block(programTable->globalScope, asmBlock);
    temps = temp_list_new();

    size_t globalTacIndex = 0;
    size_t globalTempNum = 0;

    struct AST *programRunner = program;
    while (programRunner != NULL)
    {
        switch (programRunner->type)
        {
        case T_VARIABLE_DECLARATION:
            // walk_variable_declaration sets isGlobal for us by checking if there is no parent scope
            walk_variable_declaration(programRunner, globalBlock, programTable->globalScope, &globalTacIndex, &globalTempNum, 0, A_PUBLIC);
            break;

        case T_EXTERN:
        {
            struct VariableEntry *declaredVariable = walk_variable_declaration(programRunner->child, globalBlock, programTable->globalScope, &globalTacIndex, &globalTempNum, 0, A_PUBLIC);
            declaredVariable->isExtern = 1;
        }
        break;

        case T_STRUCT:
            walk_struct_declaration(programRunner, globalBlock, programTable->globalScope);
            break;

        case T_ENUM:
            walk_enum_declaration(programRunner, globalBlock, programTable->globalScope);
            break;

        case T_ASSIGN:
            walk_assignment(programRunner, globalBlock, programTable->globalScope, &globalTacIndex, &globalTempNum);
            break;

        case T_IMPL:
            walk_implementation_block(programRunner, programTable->globalScope);
            break;

        case T_FUN:
            walk_function_declaration(programRunner, programTable->globalScope, NULL, A_PUBLIC);
            break;

        // ignore asm blocks
        case T_ASM:
            walk_asm_block(programRunner, asmBlock, programTable->globalScope, &globalTacIndex, &globalTempNum);
            break;

        default:
            InternalError("Malformed AST in WalkProgram: got %s with type %s",
                          programRunner->value,
                          get_token_name(programRunner->type));
            break;
        }
        programRunner = programRunner->sibling;
    }

    symbol_table_decay_arrays(programTable);

    return programTable;
}

struct TACOperand *get_addr_of_operand(struct AST *tree,
                                    struct BasicBlock *block,
                                    struct Scope *scope,
                                    size_t *TACIndex,
                                    size_t *tempNum,
                                    struct TACOperand *getAddrOf)
{
    struct TACLine *addrOfLine = new_tac_line(TT_ADDROF, tree);
    addrOfLine->operands[1] = *getAddrOf;

    populate_tac_operand_as_temp(&addrOfLine->operands[0], tempNum);

    *tac_get_type_of_operand(addrOfLine, 0) = *tac_get_type_of_operand(addrOfLine, 1);
    tac_get_type_of_operand(addrOfLine, 0)->pointerLevel++;
    basic_block_append(block, addrOfLine, TACIndex);

    return &addrOfLine->operands[0];
}

void walk_type_name(struct AST *tree, struct Scope *scope, struct Type *populateTypeTo)
{
    log_tree(LOG_DEBUG, tree, "WalkTypeName");
    if (tree->type != T_TYPE_NAME)
    {
        InternalError("Wrong AST (%s) passed to WalkTypeName!", get_token_name(tree->type));
    }

    type_init(populateTypeTo);

    struct AST complexTypeNameTree = {0};
    enum BASIC_TYPES basicType = VT_NULL;
    char *complexTypeName = NULL;

    switch (tree->child->type)
    {
    case T_ANY:
        basicType = VT_ANY;
        break;

    case T_U8:
        basicType = VT_U8;
        break;

    case T_U16:
        basicType = VT_U16;
        break;

    case T_U32:
        basicType = VT_U32;
        break;

    case T_U64:
        basicType = VT_U64;
        break;

    case T_IDENTIFIER:
    {

        complexTypeNameTree = *tree->child;
        complexTypeName = complexTypeNameTree.value;

        struct ScopeMember *namedType = scope_lookup(scope, complexTypeName);

        if (namedType == NULL)
        {
            log_tree(LOG_FATAL, &complexTypeNameTree, "%s does not name a type", complexTypeName);
        }

        switch (namedType->type)
        {
        case E_STRUCT:
            basicType = VT_STRUCT;
            break;

        case E_ENUM:
            basicType = VT_ENUM;
            break;

        default:
            log_tree(LOG_FATAL, &complexTypeNameTree, "%s does not name a type", complexTypeName);
        }

        if (complexTypeNameTree.type != T_IDENTIFIER)
        {
            log_tree(ERROR_INTERNAL,
                    &complexTypeNameTree,
                    "Malformed AST seen in declaration!\nExpected struct name as child of \"struct\", saw %s (%s)!",
                    complexTypeNameTree.value,
                    get_token_name(complexTypeNameTree.type));
        }
    }
    break;

    case T_CAP_SELF:
        basicType = VT_STRUCT;
        if (scope->parentImpl == NULL)
        {
            log_tree(LOG_FATAL, tree->child, "Use of 'Self' outside of impl scope!");
        }
        complexTypeName = scope->parentImpl->name;

        // construct a fake struct name tree which contains the source location info
        complexTypeNameTree = *tree->child;
        complexTypeNameTree.child = NULL;
        complexTypeNameTree.sibling = NULL;
        complexTypeNameTree.type = T_IDENTIFIER;
        complexTypeNameTree.value = complexTypeName;
        break;

    default:
        log_tree(LOG_FATAL, tree, "Malformed AST seen in declaration!");
    }

    struct AST *declaredArray = NULL;
    type_set_basic_type(populateTypeTo, basicType, complexTypeName, scrape_pointers(tree->child, &declaredArray));

    // if declaring something with the 'any' type, make sure it's only as a pointer (as its intended use is to point to unstructured data)
    if (populateTypeTo->basicType == VT_ARRAY || populateTypeTo->basicType == VT_ANY)
    {
        struct Type anyCheckRunner = *populateTypeTo;
        while (anyCheckRunner.basicType == VT_ARRAY)
        {
            anyCheckRunner = *anyCheckRunner.array.type;
        }

        if ((anyCheckRunner.pointerLevel == 0) && (anyCheckRunner.basicType == VT_ANY))
        {
            if (populateTypeTo->basicType == VT_ARRAY)
            {
                log_tree(LOG_FATAL, declaredArray, "Use of the type 'any' in arrays is forbidden!\n'any' is meant to represent unstructured data as a pointer type only\n(declare as 'any *', 'any **', etc...)");
            }
            else
            {
                log_tree(LOG_FATAL, tree->child, "Use of the type 'any' without indirection is forbidden!\n'any' is meant to represent unstructured data as a pointer type only\n(declare as 'any *', 'any **', etc...)");
            }
        }
    }

    // don't allow declaration of variables of undeclared struct or array of undeclared struct (except pointers)
    if ((populateTypeTo->basicType == VT_STRUCT) && (populateTypeTo->pointerLevel == 0))
    {
        // the lookup will bail out if an attempt is made to use an undeclared struct
        lookup_struct(scope, &complexTypeNameTree);
    }

    // if we are declaring an array, set the string with the size as the second operand
    if (declaredArray != NULL)
    {
        if (declaredArray->type != T_ARRAY_INDEX)
        {
            log_tree(LOG_FATAL, declaredArray, "Unexpected AST at end of pointer declarations!");
        }
        char *arraySizeString = declaredArray->child->value;
        // TODO: abstract this
        int declaredArraySize = atoi(arraySizeString);

        struct Type *arrayedType = dictionary_lookup_or_insert(typeDict, populateTypeTo);

        // TODO: multidimensional array declarations
        populateTypeTo->basicType = VT_ARRAY;
        populateTypeTo->array.size = declaredArraySize;
        populateTypeTo->array.type = arrayedType;
        populateTypeTo->array.initializeArrayTo = NULL;
    }
}

struct VariableEntry *walk_variable_declaration(struct AST *tree,
                                              struct BasicBlock *block,
                                              struct Scope *scope,
                                              const size_t *TACIndex,
                                              const size_t *tempNum,
                                              u8 isArgument,
                                              enum ACCESS accessibility)
{
    log_tree(LOG_DEBUG, tree, "walk_variable_declaration");

    if (tree->type != T_VARIABLE_DECLARATION)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_variable_declaration!", get_token_name(tree->type));
    }

    struct Type declaredType;

    /* 'struct' trees' children are the struct name
     * other variables' children are the pointer or variable name
     * so we need to start at tree->child for non-struct or tree->child->sibling for structs
     */

    if (tree->child->type != T_TYPE_NAME)
    {
        log_tree(LOG_FATAL, tree->child, "Malformed AST seen in declaration!");
    }

    walk_type_name(tree->child, scope, &declaredType);

    // automatically set as a global if there is no parent scope (declaring at the outermost scope)
    struct VariableEntry *declaredVariable = create_variable(scope,
                                                            tree->child->sibling,
                                                            &declaredType,
                                                            (u8)(scope->parentScope == NULL),
                                                            *TACIndex,
                                                            isArgument,
                                                            accessibility);

    return declaredVariable;
}

void walk_argument_declaration(struct AST *tree,
                             struct BasicBlock *block,
                             size_t *TACIndex,
                             size_t *tempNum,
                             struct FunctionEntry *fun)
{
    log_tree(LOG_DEBUG, tree, "WalkArgumentDeclaration");

    struct VariableEntry *declaredArgument = walk_variable_declaration(tree, block, fun->mainScope, TACIndex, tempNum, 1, A_PUBLIC);

    stack_push(fun->arguments, declaredArgument);
}

void verify_function_signatures(struct AST *tree, struct FunctionEntry *existingFunc, struct FunctionEntry *parsedFunc)
{
    // nothing to do if no existing function
    if (existingFunc == NULL)
    {
        return;
    }
    // check that if a prototype declaration exists, that our parsed declaration matches it exactly
    u8 mismatch = 0;

    if ((type_compare(&parsedFunc->returnType, &existingFunc->returnType)))
    {
        mismatch = 1;
    }

    // ensure we have both the same number of bytes of arguments and same number of arguments
    if (!mismatch &&
        (existingFunc->arguments->size == parsedFunc->arguments->size))
    {
        // if we have same number of bytes and same number, ensure everything is exactly the same
        for (size_t argIndex = 0; argIndex < existingFunc->arguments->size; argIndex++)
        {
            struct VariableEntry *existingArg = existingFunc->arguments->data[argIndex];
            struct VariableEntry *parsedArg = parsedFunc->arguments->data[argIndex];
            // ensure all arguments in order have same name, type, indirection level
            if ((strcmp(existingArg->name, parsedArg->name) != 0) ||
                (type_compare(&existingArg->type, &parsedArg->type)))
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

        char *existingReturnType = type_get_name(&existingFunc->returnType);
        printf("\t%s %s(", existingReturnType, existingFunc->name);
        free(existingReturnType);
        for (size_t argIndex = 0; argIndex < existingFunc->arguments->size; argIndex++)
        {
            struct VariableEntry *existingArg = existingFunc->arguments->data[argIndex];

            char *argType = type_get_name(&existingArg->type);
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
        char *parsedReturnType = type_get_name(&parsedFunc->returnType);
        printf("\n\t%s %s(", parsedReturnType, parsedFunc->name);
        free(parsedReturnType);
        for (size_t argIndex = 0; argIndex < parsedFunc->arguments->size; argIndex++)
        {
            struct VariableEntry *parsedArg = parsedFunc->arguments->data[argIndex];

            char *argType = type_get_name(&parsedArg->type);
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

        log_tree(LOG_FATAL, tree, " ");
    }
}

struct FunctionEntry *walk_function_declaration(struct AST *tree,
                                              struct Scope *scope,
                                              struct StructEntry *methodOf,
                                              enum ACCESS accessibility)
{
    log_tree(LOG_DEBUG, tree, "walk_function_declaration");

    if (tree->type != T_FUN)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_function_declaration!", get_token_name(tree->type));
    }

    // skip past the argumnent declarations to the return type declaration
    struct AST *returnTypeTree = tree->child;

    // functions return nothing in the default case
    struct Type returnType;
    memset(&returnType, 0, sizeof(struct Type));

    struct AST *functionNameTree = NULL;

    // if the function returns something, its return type will be the first child of the 'fun' token
    if (returnTypeTree->type == T_TYPE_NAME)
    {
        walk_type_name(returnTypeTree, scope, &returnType);

        functionNameTree = returnTypeTree->sibling;
    }
    else
    {
        // there actually is no return type tree, we just go directly to argument declarations
        functionNameTree = returnTypeTree;
    }

    // child is the lparen, function name is the child of the lparen
    struct ScopeMember *lookedUpFunction = scope_lookup(scope, functionNameTree->value);
    struct FunctionEntry *parsedFunc = NULL;
    struct FunctionEntry *existingFunc = NULL;
    struct FunctionEntry *returnedFunc = NULL;

    if (lookedUpFunction != NULL)
    {
        existingFunc = lookedUpFunction->entry;
        returnedFunc = existingFunc;
        parsedFunc = function_entry_new(scope, functionNameTree, &returnType, methodOf);
    }
    else
    {
        parsedFunc = create_function(scope, functionNameTree, &returnType, methodOf, accessibility);
        returnedFunc = parsedFunc;
    }

    if (type_is_object(&returnType))
    {
        struct Type outPointerType = returnType;
        outPointerType.pointerLevel++;
        struct AST outPointerTree = *tree;
        outPointerTree.type = T_IDENTIFIER;
        outPointerTree.value = OUT_OBJECT_POINTER_NAME;
        outPointerTree.child = NULL;
        outPointerTree.sibling = NULL;
        struct VariableEntry *outPointerArgument = create_variable(parsedFunc->mainScope, &outPointerTree, &outPointerType, 0, 0, 1, A_PUBLIC);

        stack_push(parsedFunc->arguments, outPointerArgument);
    }

    struct AST *argumentRunner = functionNameTree->sibling;
    size_t tacIndex = 0;
    size_t tempNum = 0;
    struct BasicBlock *block = basic_block_new(0);
    while ((argumentRunner != NULL) && (argumentRunner->type != T_COMPOUND_STATEMENT) && (argumentRunner->type != T_ASM))
    {
        switch (argumentRunner->type)
        {
        // looking at argument declarations
        case T_VARIABLE_DECLARATION:
        {
            walk_argument_declaration(argumentRunner, block, &tacIndex, &tempNum, parsedFunc);
        }
        break;

        case T_SELF:
        {
            if (methodOf == NULL)
            {
                log_tree(LOG_FATAL, argumentRunner, "Malformed AST within function declaration - saw self when methodOf == NULL");
            }
            struct Type selfType;
            type_init(&selfType);
            type_set_basic_type(&selfType, VT_STRUCT, methodOf->name, 1);
            struct VariableEntry *selfArgument = create_variable(parsedFunc->mainScope, argumentRunner, &selfType, 0, 0, 1, A_PUBLIC);

            stack_push(parsedFunc->arguments, selfArgument);
        }
        break;

        default:
            InternalError("Malformed AST within function - expected function name and main scope only!\nMalformed node was of type %s with value [%s]", get_token_name(argumentRunner->type), argumentRunner->value);
        }
        argumentRunner = argumentRunner->sibling;
    }

    verify_function_signatures(tree, existingFunc, parsedFunc);

    // free the basic block we used to Walk declarations of arguments
    basic_block_free(block);

    struct AST *definition = argumentRunner;
    if (definition != NULL)
    {
        if (existingFunc != NULL)
        {
            function_entry_free(parsedFunc);
            existingFunc->isDefined = 1;
            walk_function_definition(definition, existingFunc);
        }
        else
        {
            parsedFunc->isDefined = 1;
            walk_function_definition(definition, parsedFunc);
        }
    }

    return returnedFunc;
}

void walk_function_definition(struct AST *tree,
                            struct FunctionEntry *fun)
{
    log_tree(LOG_DEBUG, tree, "walk_function_definition");

    if ((tree->type != T_COMPOUND_STATEMENT) && (tree->type != T_ASM))
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_function_definition!", get_token_name(tree->type));
    }

    size_t tacIndex = 0;
    size_t tempNum = 0;
    ssize_t labelNum = 1;
    struct BasicBlock *block = basic_block_new(0);
    scope_add_basic_block(fun->mainScope, block);

    if (tree->type == T_COMPOUND_STATEMENT)
    {
        walk_scope(tree, block, fun->mainScope, &tacIndex, &tempNum, &labelNum, -1);
    }
    else
    {
        fun->isAsmFun = 1;
        walk_asm_block(tree, block, fun->mainScope, &tacIndex, &tempNum);
    }
}

void walk_method(struct AST *tree,
                struct StructEntry *methodOf,
                enum ACCESS accessibility)
{
    log(LOG_DEBUG, "walk_method", tree->sourceFile, tree->sourceLine, tree->sourceCol);

    if (tree->type != T_FUN)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_method!", get_token_name(tree->type));
    }

    struct FunctionEntry *walkedMethod = walk_function_declaration(tree, methodOf->members, methodOf, accessibility);
    walkedMethod->mainScope->parentImpl = methodOf;

    if (walkedMethod->arguments->size > 0)
    {
        struct VariableEntry *potentialSelfArg = walkedMethod->arguments->data[0];

        // if the first arg to the function is the address of a struct which we are returning
        // try and see if the second argument is self (if it exists)
        if (!strcmp(potentialSelfArg->name, OUT_OBJECT_POINTER_NAME) && (walkedMethod->arguments->size > 1))
        {
            potentialSelfArg = walkedMethod->arguments->data[1];
        }

        if ((potentialSelfArg->type.basicType == VT_STRUCT) && (strcmp(potentialSelfArg->type.nonArray.complexType.name, methodOf->name) == 0))
        {
            if (strcmp(potentialSelfArg->name, "self") == 0)
            {
                walkedMethod->isMethod = true;
            }
        }
    }
}

void walk_implementation_block(struct AST *tree, struct Scope *scope)
{
    log(LOG_DEBUG, "WalkImplementation", tree->sourceFile, tree->sourceLine, tree->sourceCol);

    if (tree->type != T_IMPL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to WalkImplementation!", get_token_name(tree->type));
    }

    struct AST *implementedStructTree = tree->child;
    if (implementedStructTree->type != T_IDENTIFIER)
    {
        log_tree(LOG_FATAL, implementedStructTree, "Malformed AST seen in WalkImplementation!");
    }

    struct StructEntry *implementedStruct = lookup_struct(scope, implementedStructTree);

    struct AST *implementationRunner = implementedStructTree->sibling;
    while (implementationRunner != NULL)
    {
        switch (implementationRunner->type)
        {
        case T_FUN:
            walk_method(implementationRunner, implementedStruct, A_PRIVATE);
            break;

        case T_PUBLIC:
            walk_method(implementationRunner->child, implementedStruct, A_PUBLIC);
            break;

        default:
            log_tree(LOG_FATAL, implementationRunner, "Malformed AST seen %s (%s) in WalkImplementation!", get_token_name(implementationRunner->type), implementationRunner->value);
        }
        implementationRunner = implementationRunner->sibling;
    }
}

void walk_struct_declaration(struct AST *tree,
                           struct BasicBlock *block,
                           struct Scope *scope)
{
    log_tree(LOG_DEBUG, tree, "walk_struct_declaration");

    if (tree->type != T_STRUCT)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to WalkStructDefinition!", get_token_name(tree->type));
    }
    size_t dummyNum = 0;

    struct StructEntry *declaredStruct = create_struct(scope, tree->child->value);

    struct AST *structBody = tree->child->sibling;

    if (structBody->type != T_STRUCT_BODY)
    {
        log_tree(LOG_FATAL, tree, "Malformed AST seen in WalkStructDefinition!");
    }

    struct AST *structBodyRunner = structBody->child;
    while (structBodyRunner != NULL)
    {
        switch (structBodyRunner->type)
        {
        case T_VARIABLE_DECLARATION:
        {
            struct VariableEntry *declaredMember = walk_variable_declaration(structBodyRunner, block, declaredStruct->members, &dummyNum, &dummyNum, 0, A_PRIVATE);
            assign_offset_to_member_variable(declaredStruct, declaredMember);
        }
        break;

        case T_PUBLIC:
        {
            struct VariableEntry *declaredMember = walk_variable_declaration(structBodyRunner->child, block, declaredStruct->members, &dummyNum, &dummyNum, 0, A_PUBLIC);
            assign_offset_to_member_variable(declaredStruct, declaredMember);
        }
        break;

        default:
            log_tree(LOG_FATAL, structBodyRunner, "Wrong AST (%s) seen in body of struct definition!", get_token_name(structBodyRunner->type));
        }

        structBodyRunner = structBodyRunner->sibling;
    }
}

void walk_enum_declaration(struct AST *tree,
                         struct BasicBlock *block,
                         struct Scope *scope)
{
    log_tree(LOG_DEBUG, tree, "walk_enum_declaration");

    if (tree->type != T_ENUM)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_enum_declaration!", get_token_name(tree->type));
    }

    struct AST *enumName = tree->child;

    struct EnumEntry *declaredEnum = create_enum(scope, enumName->value);

    for (struct AST *enumRunner = enumName->sibling; enumRunner != NULL; enumRunner = enumRunner->sibling)
    {
        if (enumRunner->type != T_IDENTIFIER)
        {
            InternalError("Malformed AST (%s) seen while Walking enum element delcarations", get_token_name(enumRunner->type));
        }

        add_enum_member(declaredEnum, enumRunner);
    }
}

void walk_return(struct AST *tree,
                struct Scope *scope,
                struct BasicBlock *block,
                size_t *TACIndex,
                size_t *tempNum)
{
    if (scope->parentFunction == NULL)
    {
        log_tree(LOG_FATAL, tree, "'return' statements are only allowed within functions");
    }

    // if the program uses a bare return statement in a function which returns a value, error out
    if ((scope->parentFunction->returnType.basicType != VT_NULL) && (tree->child == NULL))
    {
        log_tree(LOG_FATAL, tree, "No expression after return statement in function %s returning %s", scope->parentFunction->name, type_get_name(&scope->parentFunction->returnType));
    }

    struct TACLine *returnLine = new_tac_line(TT_RETURN, tree);

    if (tree->child != NULL)
    {
        walk_sub_expression(tree->child, block, scope, TACIndex, tempNum, &returnLine->operands[0]);

        if (type_compare_allow_implicit_widening(tac_get_type_of_operand(returnLine, 0), &scope->parentFunction->returnType))
        {
            char *expectedReturnType = type_get_name(&scope->parentFunction->returnType);
            char *actualReturnType = type_get_name(tac_get_type_of_operand(returnLine, 0));
            log_tree(LOG_FATAL, tree->child, "Returned type %s does not match expected return type of %s", actualReturnType, expectedReturnType);
        }

        if (type_is_object(&scope->parentFunction->returnType))
        {
            struct TACOperand *copiedFrom = &returnLine->operands[0];
            struct TACOperand addressCopiedTo;
            struct VariableEntry *outStructPointer = lookup_var_by_string(scope, OUT_OBJECT_POINTER_NAME);
            populate_tac_operand_from_variable(&addressCopiedTo, outStructPointer);

            struct TACLine *structReturnWrite = new_tac_line(TT_STORE, tree);
            structReturnWrite->operands[1] = *copiedFrom;
            populate_tac_operand_from_variable(&structReturnWrite->operands[0], outStructPointer);

            basic_block_append(block, structReturnWrite, TACIndex);

            memset(&returnLine->operands[0], 0, sizeof(struct TACOperand));
        }
    }

    basic_block_append(block, returnLine, TACIndex);

    if (tree->sibling != NULL)
    {
        log_tree(LOG_FATAL, tree->sibling, "Code after return statement is unreachable!");
    }
}

void walk_statement(struct AST *tree,
                   struct BasicBlock **blockP,
                   struct Scope *scope,
                   size_t *TACIndex,
                   size_t *tempNum,
                   ssize_t *labelNum,
                   ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "WalkStatement");

    switch (tree->type)
    {
    case T_VARIABLE_DECLARATION:
        walk_variable_declaration(tree, *blockP, scope, TACIndex, tempNum, 0, A_PUBLIC);
        break;

    case T_EXTERN:
        log_tree(LOG_FATAL, tree, "'extern' is only allowed at the global scope.");
        break;

    case T_ASSIGN:
        walk_assignment(tree, *blockP, scope, TACIndex, tempNum);
        break;

    case T_PLUS_EQUALS:
    case T_MINUS_EQUALS:
    case T_TIMES_EQUALS:
    case T_DIVIDE_EQUALS:
    case T_MODULO_EQUALS:
    case T_BITWISE_AND_EQUALS:
    case T_BITWISE_OR_EQUALS:
    case T_BITWISE_XOR_EQUALS:
    case T_LSHIFT_EQUALS:
    case T_RSHIFT_EQUALS:
        walk_arithmetic_assignment(tree, *blockP, scope, TACIndex, tempNum);
        break;

    case T_WHILE:
    {
        struct BasicBlock *afterWhileBlock = basic_block_new((*labelNum)++);
        walk_while_loop(tree, *blockP, scope, TACIndex, tempNum, labelNum, afterWhileBlock->labelNum);
        *blockP = afterWhileBlock;
        scope_add_basic_block(scope, afterWhileBlock);
    }
    break;

    case T_IF:
    {
        struct BasicBlock *afterIfBlock = basic_block_new((*labelNum)++);
        walk_if_statement(tree, *blockP, scope, TACIndex, tempNum, labelNum, afterIfBlock->labelNum);
        *blockP = afterIfBlock;
        scope_add_basic_block(scope, afterIfBlock);
    }
    break;

    case T_FOR:
    {
        struct BasicBlock *afterForBlock = basic_block_new((*labelNum)++);
        walk_for_loop(tree, *blockP, scope, TACIndex, tempNum, labelNum, afterForBlock->labelNum);
        *blockP = afterForBlock;
        scope_add_basic_block(scope, afterForBlock);
    }
    break;

    case T_MATCH:
    {
        struct BasicBlock *afterMatchBlock = basic_block_new((*labelNum)++);
        walk_match_statement(tree, *blockP, scope, TACIndex, tempNum, labelNum, afterMatchBlock->labelNum);
        *blockP = afterMatchBlock;
        scope_add_basic_block(scope, afterMatchBlock);
    }
    break;

    case T_FUNCTION_CALL:
        walk_function_call(tree, *blockP, scope, TACIndex, tempNum, NULL);
        break;

    case T_METHOD_CALL:
        walk_method_call(tree, *blockP, scope, TACIndex, tempNum, NULL);
        break;

    // subscope
    case T_COMPOUND_STATEMENT:
    {
        // TODO: is there a bug here for simple scopes within code (not attached to if/while/etc... statements? TAC dump for the scopes test seems to indicate so?)
        struct Scope *subScope = scope_create_sub_scope(scope);
        struct BasicBlock *afterSubScopeBlock = basic_block_new((*labelNum)++);
        walk_scope(tree, *blockP, subScope, TACIndex, tempNum, labelNum, afterSubScopeBlock->labelNum);
        *blockP = afterSubScopeBlock;
        scope_add_basic_block(scope, afterSubScopeBlock);
    }
    break;

    case T_RETURN:
        walk_return(tree, scope, *blockP, TACIndex, tempNum);
        break;

    case T_ASM:
        walk_asm_block(tree, *blockP, scope, TACIndex, tempNum);
        break;

    default:
        log_tree(LOG_FATAL, tree, "Unexpected AST type (%s - %s) seen in WalkStatement!", get_token_name(tree->type), tree->value);
    }
}

void walk_scope(struct AST *tree,
               struct BasicBlock *block,
               struct Scope *scope,
               size_t *TACIndex,
               size_t *tempNum,
               ssize_t *labelNum,
               ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "walk_scope");

    if (tree->type != T_COMPOUND_STATEMENT)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_scope!", get_token_name(tree->type));
    }

    struct AST *scopeRunner = tree->child;
    while (scopeRunner != NULL)
    {
        walk_statement(scopeRunner, &block, scope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        scopeRunner = scopeRunner->sibling;
    }

    if (controlConvergesToLabel > 0)
    {
        struct TACLine *controlConvergeJmp = new_tac_line(TT_JMP, tree);
        controlConvergeJmp->operands[0].name.val = controlConvergesToLabel;
        basic_block_append(block, controlConvergeJmp, TACIndex);
    }
}

struct BasicBlock *walk_logical_operator(struct AST *tree,
                                       struct BasicBlock *block,
                                       struct Scope *scope,
                                       size_t *TACIndex,
                                       size_t *tempNum,
                                       ssize_t *labelNum,
                                       ssize_t falseJumpLabelNum)
{
    log_tree(LOG_DEBUG, tree, "WalkLogicalOperator");

    switch (tree->type)
    {
    case T_LOGICAL_AND:
    {
        // if either condition is false, immediately jump to the false label
        block = walk_condition_check(tree->child, block, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);
        block = walk_condition_check(tree->child->sibling, block, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);
    }
    break;

    case T_LOGICAL_OR:
    {
        // this block will only be hit if the first condition comes back false
        struct BasicBlock *checkSecondConditionBlock = basic_block_new((*labelNum)++);
        scope_add_basic_block(scope, checkSecondConditionBlock);

        // this is the block in which execution will end up if the condition is true
        struct BasicBlock *trueBlock = basic_block_new((*labelNum)++);
        scope_add_basic_block(scope, trueBlock);
        block = walk_condition_check(tree->child, block, scope, TACIndex, tempNum, labelNum, checkSecondConditionBlock->labelNum);

        // if we pass the first condition (don't jump to checkSecondConditionBlock), short-circuit directly to the true block
        struct TACLine *firstConditionTrueJump = new_tac_line(TT_JMP, tree->child);
        firstConditionTrueJump->operands[0].name.val = trueBlock->labelNum;
        basic_block_append(block, firstConditionTrueJump, TACIndex);

        // Walk the second condition to checkSecondConditionBlock
        block = walk_condition_check(tree->child->sibling, checkSecondConditionBlock, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);

        // jump from whatever block the second condition check ends up in (passing path) to our block
        // this ensures that regardless of which condition is true (first or second) execution always end up in the same block
        struct TACLine *secondConditionTrueJump = new_tac_line(TT_JMP, tree->child->sibling);
        secondConditionTrueJump->operands[0].name.val = trueBlock->labelNum;
        basic_block_append(block, secondConditionTrueJump, TACIndex);

        block = trueBlock;
    }
    break;

    case T_LOGICAL_NOT:
    {
        // walk_condition_check already does everything we need it to
        // so just create a block representing the opposite of the condition we are testing
        // then, tell walk_condition_check to go there if our subcondition is false (!subcondition is true)
        struct BasicBlock *inverseConditionBlock = basic_block_new((*labelNum)++);
        scope_add_basic_block(scope, inverseConditionBlock);

        block = walk_condition_check(tree->child, block, scope, TACIndex, tempNum, labelNum, inverseConditionBlock->labelNum);

        // subcondition is true (!subcondition is false), then control flow should end up at the original conditionFalseJump destination
        struct TACLine *conditionFalseJump = new_tac_line(TT_JMP, tree->child);
        conditionFalseJump->operands[0].name.val = falseJumpLabelNum;
        basic_block_append(block, conditionFalseJump, TACIndex);

        // return the tricky block we created to be jumped to when our subcondition is false, or that the condition we are linearizing at this level is true
        block = inverseConditionBlock;
    }
    break;

    default:
        InternalError("Logical operator %s (%s) not supported yet",
                      get_token_name(tree->type),
                      tree->value);
    }

    return block;
}

struct BasicBlock *walk_condition_check(struct AST *tree,
                                      struct BasicBlock *block,
                                      struct Scope *scope,
                                      size_t *TACIndex,
                                      size_t *tempNum,
                                      ssize_t *labelNum,
                                      ssize_t falseJumpLabelNum)
{
    log_tree(LOG_DEBUG, tree, "walk_condition_check");

    struct TACLine *condFalseJump = new_tac_line(TT_JMP, tree);
    condFalseJump->operands[0].name.val = falseJumpLabelNum;

    // switch once to decide the jump type
    switch (tree->type)
    {
    case T_EQUALS:
        condFalseJump->operation = TT_BNE;
        break;

    case T_NOT_EQUALS:
        condFalseJump->operation = TT_BEQ;
        break;

    case T_LESS_THAN:
        condFalseJump->operation = TT_BGEU;
        break;

    case T_GREATER_THAN:
        condFalseJump->operation = TT_BLEU;
        break;

    case T_LESS_THAN_EQUALS:
        condFalseJump->operation = TT_BGTU;
        break;

    case T_GREATER_THAN_EQUALS:
        condFalseJump->operation = TT_BLTU;
        break;

    case T_LOGICAL_AND:
    case T_LOGICAL_OR:
    case T_LOGICAL_NOT:
        block = walk_logical_operator(tree, block, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);
        break;

    default:
        condFalseJump->operation = TT_BNE;
        break;
    }

    // switch a second time to actually Walk the condition
    switch (tree->type)
    {
    // arithmetic comparisons
    case T_EQUALS:
    case T_NOT_EQUALS:
    case T_LESS_THAN:
    case T_GREATER_THAN:
    case T_LESS_THAN_EQUALS:
    case T_GREATER_THAN_EQUALS:
        // standard operators (==, !=, <, >, <=, >=)
        {
            switch (tree->child->type)
            {
            case T_LOGICAL_AND:
            case T_LOGICAL_OR:
            case T_LOGICAL_NOT:
                log_tree(LOG_FATAL, tree->child, "Use of comparison operators on results of logical operators is not supported!");
                break;

            default:
                walk_sub_expression(tree->child, block, scope, TACIndex, tempNum, &condFalseJump->operands[1]);
                break;
            }

            switch (tree->child->sibling->type)
            {
            case T_LOGICAL_AND:
            case T_LOGICAL_OR:
            case T_LOGICAL_NOT:
                log_tree(LOG_FATAL, tree->child->sibling, "Use of comparison operators on results of logical operators is not supported!");
                break;

            default:
                walk_sub_expression(tree->child->sibling, block, scope, TACIndex, tempNum, &condFalseJump->operands[2]);
                break;
            }
        }
        break;

    case T_LOGICAL_AND:
    case T_LOGICAL_OR:
    case T_LOGICAL_NOT:
        free(condFalseJump);
        condFalseJump = NULL;
        break;

    case T_IDENTIFIER:
    case T_ADD:
    case T_SUBTRACT:
    case T_MULTIPLY:
    case T_DIVIDE:
    case T_MODULO:
    case T_LSHIFT:
    case T_RSHIFT:
    case T_BITWISE_AND:
    case T_BITWISE_OR:
    case T_BITWISE_NOT:
    case T_BITWISE_XOR:
    case T_DEREFERENCE:
    case T_ADDRESS_OF:
    case T_CAST:
    case T_DOT:
    case T_FUNCTION_CALL:
    {
        condFalseJump->operation = TT_BEQ;
        walk_sub_expression(tree, block, scope, TACIndex, tempNum, &condFalseJump->operands[1]);

        condFalseJump->operands[2].type.basicType = VT_U8;
        condFalseJump->operands[2].permutation = VP_LITERAL;
        condFalseJump->operands[2].name.str = "0";
    }
    break;

    default:
    {
        InternalError("Comparison operator %s (%s) not supported yet",
                      get_token_name(tree->type),
                      tree->value);
    }
    break;
    }

    if (condFalseJump != NULL)
    {
        basic_block_append(block, condFalseJump, TACIndex);
    }
    return block;
}

void walk_while_loop(struct AST *tree,
                   struct BasicBlock *block,
                   struct Scope *scope,
                   size_t *TACIndex,
                   size_t *tempNum,
                   ssize_t *labelNum,
                   ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "walk_while_loop");

    if (tree->type != T_WHILE)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_while_loop!", get_token_name(tree->type));
    }

    struct BasicBlock *beforeWhileBlock = block;

    struct TACLine *enterWhileJump = new_tac_line(TT_JMP, tree);
    enterWhileJump->operands[0].name.val = *labelNum;
    basic_block_append(beforeWhileBlock, enterWhileJump, TACIndex);

    // create a subscope from which we will work
    struct Scope *whileScope = scope_create_sub_scope(scope);
    struct BasicBlock *whileBlock = basic_block_new((*labelNum)++);
    scope_add_basic_block(whileScope, whileBlock);

    struct TACLine *whileDo = new_tac_line(TT_DO, tree);
    basic_block_append(whileBlock, whileDo, TACIndex);

    whileBlock = walk_condition_check(tree->child, whileBlock, whileScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);

    ssize_t endWhileLabel = (*labelNum)++;

    struct AST *whileBody = tree->child->sibling;
    if (whileBody->type == T_COMPOUND_STATEMENT)
    {
        walk_scope(whileBody, whileBlock, whileScope, TACIndex, tempNum, labelNum, endWhileLabel);
    }
    else
    {
        walk_statement(whileBody, &whileBlock, whileScope, TACIndex, tempNum, labelNum, endWhileLabel);
    }

    struct TACLine *whileLoopJump = new_tac_line(TT_JMP, tree);
    whileLoopJump->operands[0].name.val = enterWhileJump->operands[0].name.val;

    block = basic_block_new(endWhileLabel);
    scope_add_basic_block(scope, block);

    struct TACLine *whileEndDo = new_tac_line(TT_ENDDO, tree);
    basic_block_append(block, whileLoopJump, TACIndex);
    basic_block_append(block, whileEndDo, TACIndex);
}

void walk_if_statement(struct AST *tree,
                     struct BasicBlock *block,
                     struct Scope *scope,
                     size_t *TACIndex,
                     size_t *tempNum,
                     ssize_t *labelNum,
                     ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "walk_if_statement");

    if (tree->type != T_IF)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_if_statement!", get_token_name(tree->type));
    }

    // if we have an else block
    if (tree->child->sibling->sibling != NULL)
    {
        ssize_t elseLabel = (*labelNum)++;
        block = walk_condition_check(tree->child, block, scope, TACIndex, tempNum, labelNum, elseLabel);

        struct Scope *ifScope = scope_create_sub_scope(scope);
        struct BasicBlock *ifBlock = basic_block_new((*labelNum)++);
        scope_add_basic_block(ifScope, ifBlock);

        struct TACLine *enterIfJump = new_tac_line(TT_JMP, tree);
        enterIfJump->operands[0].name.val = ifBlock->labelNum;
        basic_block_append(block, enterIfJump, TACIndex);

        struct AST *ifBody = tree->child->sibling;
        if (ifBody->type == T_COMPOUND_STATEMENT)
        {
            walk_scope(ifBody, ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
        else
        {
            walk_statement(ifBody, &ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }

        struct Scope *elseScope = scope_create_sub_scope(scope);
        struct BasicBlock *elseBlock = basic_block_new(elseLabel);
        scope_add_basic_block(elseScope, elseBlock);

        struct AST *elseBody = tree->child->sibling->sibling;
        if (elseBody->type == T_COMPOUND_STATEMENT)
        {
            walk_scope(elseBody, elseBlock, elseScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
        else
        {
            walk_statement(elseBody, &elseBlock, elseScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
    }
    // no else block
    else
    {
        block = walk_condition_check(tree->child, block, scope, TACIndex, tempNum, labelNum, controlConvergesToLabel);

        struct Scope *ifScope = scope_create_sub_scope(scope);
        struct BasicBlock *ifBlock = basic_block_new((*labelNum)++);
        scope_add_basic_block(ifScope, ifBlock);

        struct TACLine *enterIfJump = new_tac_line(TT_JMP, tree);
        enterIfJump->operands[0].name.val = ifBlock->labelNum;
        basic_block_append(block, enterIfJump, TACIndex);

        struct AST *ifBody = tree->child->sibling;
        if (ifBody->type == T_COMPOUND_STATEMENT)
        {
            walk_scope(ifBody, ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
        else
        {
            walk_statement(ifBody, &ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
    }
}

void walk_for_loop(struct AST *tree,
                 struct BasicBlock *block,
                 struct Scope *scope,
                 size_t *TACIndex,
                 size_t *tempNum,
                 ssize_t *labelNum,
                 ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "walk_for_loop");

    if (tree->type != T_FOR)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_for_loop!", get_token_name(tree->type));
    }

    struct Scope *forScope = scope_create_sub_scope(scope);
    struct BasicBlock *beforeForBlock = basic_block_new((*labelNum)++);
    scope_add_basic_block(forScope, beforeForBlock);

    struct TACLine *enterForScopeJump = new_tac_line(TT_JMP, tree);
    enterForScopeJump->operands[0].name.val = beforeForBlock->labelNum;
    basic_block_append(block, enterForScopeJump, tempNum);

    struct AST *forStartExpression = tree->child;
    struct AST *forCondition = tree->child->sibling;

    //                       for   e1      e2       e3
    struct AST *forAction = tree->child->sibling->sibling;
    // if the third expression has no sibling, it isn't actually a third expression but the body (and there is no third expression)
    if (forAction->sibling == NULL)
    {
        forAction = NULL;
    }
    walk_statement(forStartExpression, &beforeForBlock, forScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);

    struct TACLine *enterForJump = new_tac_line(TT_JMP, tree);
    enterForJump->operands[0].name.val = *labelNum;
    basic_block_append(beforeForBlock, enterForJump, TACIndex);

    // create a subscope from which we will work
    struct BasicBlock *forBlock = basic_block_new((*labelNum)++);

    struct TACLine *whileDo = new_tac_line(TT_DO, tree);
    basic_block_append(forBlock, whileDo, TACIndex);
    scope_add_basic_block(forScope, forBlock);

    forBlock = walk_condition_check(forCondition, forBlock, forScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);

    ssize_t endForLabel = (*labelNum)++;

    struct AST *forBody = tree->child;
    while (forBody->sibling != NULL)
    {
        forBody = forBody->sibling;
    }
    if (forBody->type == T_COMPOUND_STATEMENT)
    {
        walk_scope(forBody, forBlock, forScope, TACIndex, tempNum, labelNum, endForLabel);
    }
    else
    {
        walk_statement(forBody, &forBlock, forScope, TACIndex, tempNum, labelNum, endForLabel);
    }

    struct BasicBlock *forActionBlock = basic_block_new(endForLabel);
    scope_add_basic_block(forScope, forActionBlock);

    if (forAction != NULL)
    {
        walk_statement(forAction, &forActionBlock, forScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
    }

    struct TACLine *forLoopJump = new_tac_line(TT_JMP, tree);
    forLoopJump->operands[0].name.val = enterForJump->operands[0].name.val;

    struct TACLine *forEndDo = new_tac_line(TT_ENDDO, tree);
    basic_block_append(forActionBlock, forLoopJump, TACIndex);
    basic_block_append(forActionBlock, forEndDo, TACIndex);
}

size_t walk_match_case_block(struct AST *statement,
                          struct Scope *scope,
                          size_t *tacIndex,
                          size_t *tempNum,
                          ssize_t *labelNum,
                          ssize_t controlConvergesToLabel)
{
    struct BasicBlock *caseBlock = basic_block_new((*labelNum)++);
    scope_add_basic_block(scope, caseBlock);
    size_t caseEntryLabel = caseBlock->labelNum;

    if (statement != NULL)
    {
        walk_statement(statement, &caseBlock, scope, tacIndex, tempNum, labelNum, controlConvergesToLabel);
    }

    // make sure every case ends up at the convergence block after the match
    struct TACLine *exitCaseJump = new_tac_line(TT_JMP, statement);
    exitCaseJump->operands[0].name.val = controlConvergesToLabel;
    basic_block_append(caseBlock, exitCaseJump, tacIndex);

    return caseEntryLabel;
}

void walk_match_statement(struct AST *tree,
                        struct BasicBlock *block,
                        struct Scope *scope,
                        size_t *tacIndex,
                        size_t *tempNum,
                        ssize_t *labelNum,
                        ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "walk_match_statement");

    if (tree->type != T_MATCH)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_match_statement!", get_token_name(tree->type));
    }

    struct AST *matchedExpression = tree->child;

    struct AST *matchRunner = matchedExpression->sibling;

    struct Set *matchedValues = set_new(sizet_pointer_compare, free);

    struct TACOperand matchedAgainst;
    walk_sub_expression(matchedExpression, block, scope, tacIndex, tempNum, &matchedAgainst);
    struct Type *matchedType = tac_operand_get_type(&matchedAgainst);
    struct EnumEntry *matchedEnum = NULL;
    if (matchedType->basicType == VT_ENUM)
    {
        matchedEnum = lookup_enum_by_type(scope, matchedType);
    }

    if (matchedType->pointerLevel == 0)
    {
        switch (matchedType->basicType)
        {
        case VT_STRUCT:
        case VT_ARRAY:
            log_tree(LOG_FATAL, matchedExpression, "Matched expression has struct type %s - can't match structs or arrays", type_get_name(matchedType));
            break;

        case VT_ANY:
        case VT_NULL:
            InternalError("Illegal type %s seen from matched expresssion linearization", type_get_name(matchedType));
            break;

        case VT_U8:
        case VT_U16:
        case VT_U32:
        case VT_U64:
        case VT_ENUM:
            break;
        }
    }

    // need a flag because in the event that the underscore case is an empty statement (semicolon) there will be no tree
    bool haveUnderscoreCase = false;
    struct AST *underscoreAction = NULL;

    while (matchRunner != NULL)
    {
        struct AST *matchArmAction = NULL;
        if (matchRunner->child->child != NULL)
        {
            matchArmAction = matchRunner->child->child;
        }

        struct AST *matchedValueRunner = matchRunner->child->sibling;

        while (matchedValueRunner != NULL)
        {
            if (matchedType->basicType == VT_ENUM)
            {
                switch (matchedValueRunner->type)
                {
                case T_UNDERSCORE:
                    if (haveUnderscoreCase)
                    {
                        log_tree(LOG_FATAL, matchRunner, "Duplicated underscore case");
                    }
                    haveUnderscoreCase = true;
                    underscoreAction = matchRunner->child;
                    break;

                case T_IDENTIFIER:
                {
                    struct EnumMember *matchedMember = lookup_enum_member(matchedEnum, matchedValueRunner);

                    if (set_find(matchedValues, &matchedMember->numerical) != NULL)
                    {
                        log_tree(LOG_FATAL, matchedValueRunner, "Duplicated match case %s", matchedValueRunner->value);
                    }

                    size_t *matchedValuePointer = malloc(sizeof(size_t));
                    *matchedValuePointer = matchedMember->numerical;
                    set_insert(matchedValues, matchedValuePointer);

                    struct TACLine *matchJump = new_tac_line(TT_BEQ, matchedValueRunner);

                    matchJump->operands[1].type = *tac_operand_get_type(&matchedAgainst);
                    matchJump->operands[1].permutation = VP_LITERAL;

                    char printedMatchedValue[sprintedNumberLength];
                    snprintf(printedMatchedValue, sprintedNumberLength - 1, "%zu", *matchedValuePointer);
                    matchJump->operands[1].name.str = dictionary_lookup_or_insert(parseDict, printedMatchedValue);

                    matchJump->operands[2] = matchedAgainst;

                    basic_block_append(block, matchJump, tacIndex);

                    matchJump->operands[0].name.val = walk_match_case_block(matchArmAction, scope, tacIndex, tempNum, labelNum, controlConvergesToLabel);
                }
                break;

                default:
                    log_tree(LOG_FATAL, matchedValueRunner, "Match against %s invalid for match against type %s", matchedValueRunner->value, type_get_name(matchedType));
                    break;
                }
            }
            else // not matching against an enum type
            {
                switch (matchedValueRunner->type)
                {
                case T_UNDERSCORE:
                    if (haveUnderscoreCase)
                    {
                        log_tree(LOG_FATAL, matchRunner, "Duplicated underscore case");
                    }
                    haveUnderscoreCase = true;
                    underscoreAction = matchRunner->child;
                    break;

                case T_CONSTANT:
                case T_CHAR_LITERAL:
                {
                    size_t matchedValue;
                    if (matchedValueRunner->type == T_CHAR_LITERAL)
                    {
                        matchedValue = matchedValueRunner->value[0];
                    }

                    else
                    {
                        if (strncmp(matchedValueRunner->value, "0x", 2) == 0)
                        {
                            matchedValue = parse_hex_constant(matchedValueRunner->value);
                        }
                        else
                        {
                            // TODO: abstract this
                            matchedValue = atoi(matchedValueRunner->value);
                        }
                    }

                    if (set_find(matchedValues, &matchedValue) != NULL)
                    {
                        log_tree(LOG_FATAL, matchedValueRunner, "Duplicated match case %s", matchedValueRunner->value);
                    }

                    size_t *matchedValuePointer = malloc(sizeof(size_t));
                    *matchedValuePointer = matchedValue;
                    set_insert(matchedValues, matchedValuePointer);

                    struct TACLine *matchJump = new_tac_line(TT_BEQ, matchedValueRunner);

                    matchJump->operands[1].type = *tac_operand_get_type(&matchedAgainst);
                    matchJump->operands[1].permutation = VP_LITERAL;

                    char printedMatchedValue[sprintedNumberLength];
                    snprintf(printedMatchedValue, sprintedNumberLength - 1, "%zu", matchedValue);
                    matchJump->operands[1].name.str = dictionary_lookup_or_insert(parseDict, printedMatchedValue);

                    matchJump->operands[2] = matchedAgainst;

                    basic_block_append(block, matchJump, tacIndex);

                    matchJump->operands[0].name.val = walk_match_case_block(matchArmAction, scope, tacIndex, tempNum, labelNum, controlConvergesToLabel);
                }
                break;

                case T_IDENTIFIER:
                    log_tree(LOG_FATAL, matchedValueRunner, "Match against identifier %s invalid for match against type %s", matchedValueRunner->value, type_get_name(matchedType));
                    break;

                default:
                    log_tree(LOG_FATAL, matchRunner, "Malformed AST (%s) seen in cases of match statement!", get_token_name(matchedValueRunner->type));
                }
            }

            matchedValueRunner = matchedValueRunner->sibling;
        }
        matchRunner = matchRunner->sibling;
    }

    if (underscoreAction != NULL)
    {
        struct TACLine *underscoreJump = new_tac_line(TT_JMP, underscoreAction);
        if (underscoreAction->child != NULL)
        {
            underscoreJump->operands[0].name.val = walk_match_case_block(underscoreAction->child, scope, tacIndex, tempNum, labelNum, controlConvergesToLabel);
        }
        else
        {
            underscoreJump->operands[0].name.val = controlConvergesToLabel;
        }
        basic_block_append(block, underscoreJump, tacIndex);
    }
    else
    {
        size_t stateSpaceSize = 0;
        switch (matchedType->basicType)
        {
        case VT_U8:
            stateSpaceSize = U8_MAX + 1;
            break;
        case VT_U16:
            stateSpaceSize = U16_MAX + 1;
            break;
        case VT_U32:
            stateSpaceSize = U32_MAX + 1;
            break;
        case VT_U64:
            // realistically there is never a worry about overflow unless someone manually writes every single match case for u64
            log_tree(LOG_FATAL, tree, "There is no conceivable way you wrote U64_MAX match cases for this match against a u64. Something is broken.");
            break;
        case VT_ENUM:
            stateSpaceSize = matchedEnum->members->elements->size;
            break;
        case VT_ANY:
            break;
        case VT_NULL:
            InternalError("VT_NULL seen as type of matched expression");
        case VT_STRUCT:
            InternalError("VT_STRUCT seen as type of matched expression");
        case VT_ARRAY:
            InternalError("VT_STRUCT seen as type of matched expression");
        }

        size_t missingCases = matchedValues->elements->size - stateSpaceSize;

        if (missingCases > 0)
        {
            char *pluralString = "";
            if (missingCases > 1)
            {
                pluralString = "s";
            }
            log_tree(LOG_FATAL, tree, "Missing %zu match case%s for type %s", stateSpaceSize - matchedValues->elements->size, pluralString, type_get_name(matchedType));
        }
    }
    set_free(matchedValues);
}

void walk_assignment(struct AST *tree,
                    struct BasicBlock *block,
                    struct Scope *scope,
                    size_t *TACIndex,
                    size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_assignment");

    if (tree->type != T_ASSIGN)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_assignment!", get_token_name(tree->type));
    }

    struct AST *lhs = tree->child;
    struct AST *rhs = tree->child->sibling;

    // don't increment the index until after we deal with nested expressions
    struct TACLine *assignment = new_tac_line(TT_ASSIGN, tree);

    struct TACOperand assignedValue;
    memset(&assignedValue, 0, sizeof(struct TACOperand));

    // if we have anything but an initializer on the RHS, Walk it as a subexpression and save for later
    if (rhs->type != T_STRUCT_INITIALIZER)
    {
        walk_sub_expression(rhs, block, scope, TACIndex, tempNum, &assignedValue);
    }

    struct VariableEntry *assignedVariable = NULL;
    switch (lhs->type)
    {
    case T_VARIABLE_DECLARATION:
        assignedVariable = walk_variable_declaration(lhs, block, scope, TACIndex, tempNum, 0, A_PUBLIC);
        populate_tac_operand_from_variable(&assignment->operands[0], assignedVariable);
        assignment->operands[1] = assignedValue;

        if (assignedVariable->type.basicType == VT_ARRAY)
        {
            char *arrayName = type_get_name(&assignedVariable->type);
            log_tree(LOG_FATAL, tree, "Assignment to local array variable %s with type %s is not allowed!", assignedVariable->name, arrayName);
        }
        break;

    case T_IDENTIFIER:
        assignedVariable = lookup_var(scope, lhs);
        populate_tac_operand_from_variable(&assignment->operands[0], assignedVariable);
        assignment->operands[1] = assignedValue;
        break;

    // TODO: generate optimized addressing modes for arithmetic
    case T_DEREFERENCE:
    {
        struct AST *writtenPointer = lhs->child;
        switch (writtenPointer->type)
        {
        case T_ADD:
        case T_SUBTRACT:
            walk_pointer_arithmetic(writtenPointer, block, scope, TACIndex, tempNum, &assignment->operands[0]);
            break;

        default:
            walk_sub_expression(writtenPointer, block, scope, TACIndex, tempNum, &assignment->operands[0]);
            break;
        }
        assignment->operation = TT_STORE;
        assignment->operands[1] = assignedValue;
    }
    break;

    case T_ARRAY_INDEX:
    {
        assignment->operation = TT_STORE;
        struct TACLine *arrayAccessLine = walk_array_ref(lhs, block, scope, TACIndex, tempNum);
        convert_load_to_lea(arrayAccessLine, &assignment->operands[0]);

        assignment->operands[1] = assignedValue;
    }
    break;

    case T_DOT:
    {
        assignment->operation = TT_STORE;
        struct TACLine *memberAccessLine = walk_member_access(lhs, block, scope, TACIndex, tempNum, &assignment->operands[0], 0);
        convert_load_to_lea(memberAccessLine, &assignment->operands[0]);

        assignment->operands[1] = assignedValue;
    }
    break;

    default:
        log_tree(LOG_FATAL, lhs, "Unexpected AST (%s) seen in walk_assignment!", lhs->value);
        break;
    }

    if (rhs->type == T_STRUCT_INITIALIZER)
    {
        walk_struct_initializer(rhs, block, scope, TACIndex, tempNum, &assignment->operands[0]);
        free(assignment);
        assignment = NULL;
    }

    if (assignment != NULL)
    {
        basic_block_append(block, assignment, TACIndex);
    }
}

void walk_arithmetic_assignment(struct AST *tree,
                              struct BasicBlock *block,
                              struct Scope *scope,
                              size_t *TACIndex,
                              size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_arithmetic_assignment");

    struct AST fakeArith = *tree;
    switch (tree->type)
    {
    case T_PLUS_EQUALS:
        fakeArith.type = T_ADD;
        fakeArith.value = "+";
        break;

    case T_MINUS_EQUALS:
        fakeArith.type = T_SUBTRACT;
        fakeArith.value = "-";
        break;

    case T_TIMES_EQUALS:
        fakeArith.type = T_MULTIPLY;
        fakeArith.value = "*";
        break;

    case T_DIVIDE_EQUALS:
        fakeArith.type = T_DIVIDE;
        fakeArith.value = "/";
        break;

    case T_MODULO_EQUALS:
        fakeArith.type = T_MODULO;
        fakeArith.value = "%";
        break;

    case T_BITWISE_AND_EQUALS:
        fakeArith.type = T_BITWISE_AND;
        fakeArith.value = "&";
        break;

    case T_BITWISE_OR_EQUALS:
        fakeArith.type = T_BITWISE_OR;
        fakeArith.value = "|";
        break;

    case T_BITWISE_XOR_EQUALS:
        fakeArith.type = T_BITWISE_XOR;
        fakeArith.value = "^";
        break;

    case T_LSHIFT_EQUALS:
        fakeArith.type = T_LSHIFT;
        fakeArith.value = "<<";
        break;

    case T_RSHIFT_EQUALS:
        fakeArith.type = T_RSHIFT;
        fakeArith.value = ">>";
        break;

    default:
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_arithmetic_assignment!", get_token_name(tree->type));
    }

    // our fake arithmetic ast will have the child of the arithmetic assignment operator
    // this effectively duplicates the LHS of the assignment to the first operand of the arithmetic operator
    struct AST *lhs = tree->child;
    fakeArith.child = lhs;

    struct AST fakelhs = *lhs;
    fakelhs.sibling = &fakeArith;

    struct AST fakeAssignment = *tree;
    fakeAssignment.value = "=";
    fakeAssignment.type = T_ASSIGN;

    fakeAssignment.child = &fakelhs;

    walk_assignment(&fakeAssignment, block, scope, TACIndex, tempNum);
}

struct TACOperand *walk_bitwise_not(struct AST *tree,
                                  struct BasicBlock *block,
                                  struct Scope *scope,
                                  size_t *TACIndex,
                                  size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "WalkBitwiseNot");

    if (tree->type != T_BITWISE_NOT)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to WalkBitwiseNot!", get_token_name(tree->type));
    }

    // generically set to TT_ADD, we will actually set the operation within switch cases
    struct TACLine *bitwiseNotLine = new_tac_line(TT_BITWISE_NOT, tree);

    populate_tac_operand_as_temp(&bitwiseNotLine->operands[0], tempNum);

    walk_sub_expression(tree->child, block, scope, TACIndex, tempNum, &bitwiseNotLine->operands[1]);
    *tac_get_type_of_operand(bitwiseNotLine, 0) = *tac_get_type_of_operand(bitwiseNotLine, 1);

    struct TACOperand *operandA = &bitwiseNotLine->operands[1];

    // TODO: consistent bitwise arithmetic checking, print type name
    if ((operandA->type.pointerLevel > 0) || (operandA->type.basicType == VT_ARRAY))
    {
        log_tree(LOG_FATAL, tree, "Bitwise arithmetic on pointers is not allowed!");
    }

    basic_block_append(block, bitwiseNotLine, TACIndex);

    return &bitwiseNotLine->operands[0];
}

void ensure_all_fields_initialized(struct AST *tree, size_t initMemberIdx, struct StructEntry *initializedStruct)
{
    // if all fields of the struct are not initialized, this is an error
    if (initMemberIdx < initializedStruct->memberLocations->size)
    {
        char *fieldsString = malloc(1);
        fieldsString[0] = '\0';

        // go through the remaining fields, construct a string with the type and name of all missing fields
        while (initMemberIdx < initializedStruct->memberLocations->size)
        {
            struct StructMemberOffset *unInitField = (struct StructMemberOffset *)initializedStruct->memberLocations->data[initMemberIdx];

            char *unInitTypeName = type_get_name(&unInitField->variable->type);
            size_t origLen = strlen(fieldsString);
            size_t addlSize = strlen(unInitTypeName) + strlen(unInitField->variable->name) + 2;
            char *separatorString = "";
            if (initMemberIdx + 1 < initializedStruct->memberLocations->size)
            {
                addlSize += 2;
                separatorString = ", ";
            }
            char *longerFieldsString = realloc(fieldsString, origLen + addlSize);
            if (longerFieldsString == NULL)
            {
                InternalError("Couldn't realloc fieldsString");
            }
            fieldsString = longerFieldsString;

            sprintf(fieldsString + origLen, "%s %s%s", unInitTypeName, unInitField->variable->name, separatorString);
            free(unInitTypeName);

            initMemberIdx++;
        }

        log_tree(LOG_FATAL, tree, "Missing initializers for member(s) of %s: %s", initializedStruct->name, fieldsString);
    }
}

void walk_struct_initializer(struct AST *tree,
                           struct BasicBlock *block,
                           struct Scope *scope,
                           size_t *TACIndex,
                           size_t *tempNum,
                           struct TACOperand *initialized)
{
    if (tree->type != T_STRUCT_INITIALIZER)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_struct_initializer!", get_token_name(tree->type));
    }

    struct Type *initializedType = tac_operand_get_type(initialized);
    // make sure we initialize only a struct or a struct*
    if (!type_is_struct_object(initializedType) && !((initializedType->basicType == VT_STRUCT) && (initializedType->pointerLevel == 1)))
    {
        log_tree(LOG_FATAL, tree, "Cannot use initializer non-struct type %s", type_get_name(initializedType));
    }

    // automagically get the address of whatever we are initializing if it is a regular struct
    // TODO: test initializing pointers directly? Is this desirable behavior like allowing struct.member for both structs and struct*s or is this nonsense?
    if (initializedType->pointerLevel == 0)
    {
        initialized = get_addr_of_operand(tree, block, scope, TACIndex, tempNum, initialized);
    }

    struct StructEntry *initializedStruct = lookup_struct_by_type(scope, initializedType);
    size_t initMemberIdx = 0;
    for (struct AST *initRunner = tree->child; initRunner != NULL; initRunner = initRunner->sibling)
    {
        // sanity check initializer parse
        if (initRunner->type != T_ASSIGN)
        {
            InternalError("Malformed AST see inside struct initializer, expected T_ASSIGN, with first child as T_IDENTIFIER, got %s with first child as %s", get_token_name(initRunner->type));
        }

        struct AST *initMemberTree = initRunner->child;
        struct AST *initToTree = initMemberTree->sibling;

        if (initMemberTree->type != T_IDENTIFIER)
        {
            InternalError("Malformed AST for initializer, expected identifier on LHS but got %s", get_token_name(initMemberTree->type));
        }

        // first, attempt to look up the member by tree in order to throw an error in the case of a nonexistent one being referenced
        struct StructMemberOffset *member = lookup_member_variable(initializedStruct, initMemberTree, scope);

        // next, check the ordering index for the field we are expecting to initialize
        struct StructMemberOffset *expectedMember = (struct StructMemberOffset *)initializedStruct->memberLocations->data[initMemberIdx];
        if ((member->offset != expectedMember->offset) || (strcmp(member->variable->name, expectedMember->variable->name) != 0))
        {
            log(LOG_FATAL, "Initializer element %zu of struct %s should be %s, not %s", initMemberIdx + 1, initializedStruct->name, expectedMember->variable->name, member->variable->name);
        }

        struct TACOperand initializedValue = {0};
        initializedValue.type = member->variable->type;

        struct TACLine *getAddrOfField = new_tac_line(TT_LEA_OFF, initRunner);
        populate_tac_operand_as_temp(&getAddrOfField->operands[0], tempNum);
        getAddrOfField->operands[0].type = member->variable->type;
        getAddrOfField->operands[0].type.pointerLevel++;

        getAddrOfField->operands[1] = *initialized;
        getAddrOfField->operands[2].type.basicType = VT_U64;
        getAddrOfField->operands[2].permutation = VP_LITERAL;
        getAddrOfField->operands[2].name.val = member->offset;
        basic_block_append(block, getAddrOfField, TACIndex);

        if (initToTree->type == T_STRUCT_INITIALIZER)
        {
            // we are initializing the field directly from its address, recurse
            initializedValue = getAddrOfField->operands[0];
            walk_struct_initializer(initToTree, block, scope, TACIndex, tempNum, &initializedValue);
        }
        else
        {
            walk_sub_expression(initToTree, block, scope, TACIndex, tempNum, &initializedValue);

            // make sure the subexpression has a sane type to be stored in the field we are initializing
            if (type_compare_allow_implicit_widening(tac_operand_get_type(&initializedValue), &member->variable->type))
            {
                log_tree(LOG_FATAL, initToTree, "Initializer expression for field %s.%s has type %s but expected type %s", initializedStruct->name, member->variable->name, type_get_name(tac_operand_get_type(&initializedValue)), type_get_name(&member->variable->type));
            }

            // direct memory write for the store of this field
            struct TACLine *storeInitializedValue = new_tac_line(TT_STORE, initRunner);
            storeInitializedValue->operands[1] = initializedValue;
            storeInitializedValue->operands[0] = getAddrOfField->operands[0];

            basic_block_append(block, storeInitializedValue, TACIndex);
            log(LOG_WARNING, "init %s.%s to %s", initializedType->nonArray.complexType.name, member->variable->name, initToTree->value);
        }

        initMemberIdx++;
    }

    ensure_all_fields_initialized(tree, initMemberIdx, initializedStruct);
}

void walk_sub_expression(struct AST *tree,
                       struct BasicBlock *block,
                       struct Scope *scope,
                       size_t *TACIndex,
                       size_t *tempNum,
                       struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_sub_expression");

    switch (tree->type)
    {
        // variable read
    case T_SELF:
    {
        struct VariableEntry *readVariable = lookup_var(scope, tree);
        populate_tac_operand_from_variable(destinationOperand, readVariable);
    }
    break;

    // identifier = variable name or enum member
    case T_IDENTIFIER:
    {
        struct EnumEntry *possibleEnum = lookup_enum_by_member_name(scope, tree->value);
        if (possibleEnum != NULL)
        {
            populate_tac_operand_from_enum_member(destinationOperand, possibleEnum, tree);
        }
        else
        {
            struct VariableEntry *readVariable = lookup_var(scope, tree);
            populate_tac_operand_from_variable(destinationOperand, readVariable);
        }
    }
    break;

    // FIXME: there exists some code path where we can reach this point with garbage in types, resulting in a crash when printing TAC operand types
    case T_CONSTANT:
        type_init(&destinationOperand->type);
        type_init(&destinationOperand->castAsType);
        destinationOperand->name.str = tree->value;
        destinationOperand->type.basicType = select_variable_type_for_literal(tree->value);
        destinationOperand->permutation = VP_LITERAL;
        break;

    case T_CHAR_LITERAL:
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
                log_tree(LOG_FATAL, tree, "Saw T_CHAR_LITERAL with escape character value of %s - expected first char to be \\!", tree->value);
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
                log_tree(LOG_FATAL, tree, "Unexpected escape character: %s", tree->value);
            }

            sprintf(literalAsNumber, "%d", escapeCharValue);
        }
        else
        {
            log_tree(LOG_FATAL, tree, "Saw T_CHAR_LITERAL with string length of %lu (value '%s')!", literalLen, tree->value);
        }

        destinationOperand->name.str = dictionary_lookup_or_insert(parseDict, literalAsNumber);
        destinationOperand->type.basicType = VT_U8;
        destinationOperand->permutation = VP_LITERAL;
    }
    break;

    case T_STRING_LITERAL:
        walk_string_literal(tree, block, scope, destinationOperand);
        break;

    case T_FUNCTION_CALL:
        walk_function_call(tree, block, scope, TACIndex, tempNum, destinationOperand);
        break;

    case T_METHOD_CALL:
        walk_method_call(tree, block, scope, TACIndex, tempNum, destinationOperand);
        break;

    case T_ASSOCIATED_CALL:
        walk_associated_call(tree, block, scope, TACIndex, tempNum, destinationOperand);
        break;

    case T_DOT:
    {
        walk_member_access(tree, block, scope, TACIndex, tempNum, destinationOperand, 0);
    }
    break;

    case T_ADD:
    case T_SUBTRACT:
    case T_MULTIPLY:
    case T_DIVIDE:
    case T_MODULO:
    case T_LSHIFT:
    case T_RSHIFT:
    case T_LESS_THAN:
    case T_GREATER_THAN:
    case T_LESS_THAN_EQUALS:
    case T_GREATER_THAN_EQUALS:
    case T_BITWISE_OR:
    case T_BITWISE_XOR:
    {
        struct TACOperand *expressionResult = walk_expression(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = *expressionResult;
    }
    break;

    case T_BITWISE_NOT:
    {
        struct TACOperand *bitwiseNotResult = walk_bitwise_not(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = *bitwiseNotResult;
    }
    break;

    // array reference
    case T_ARRAY_INDEX:
    {
        struct TACLine *arrayRefLine = walk_array_ref(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = arrayRefLine->operands[0];
    }
    break;

    case T_DEREFERENCE:
    {
        struct TACOperand *dereferenceResult = walk_dereference(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = *dereferenceResult;
    }
    break;

    case T_ADDRESS_OF:
    {
        struct TACOperand *addrOfResult = walk_addr_of(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = *addrOfResult;
    }
    break;

    case T_BITWISE_AND:
    {
        struct TACOperand *expressionResult = walk_expression(tree, block, scope, TACIndex, tempNum);
        *destinationOperand = *expressionResult;
    }
    break;

    // TODO: helper function for casting - can better enforce validity of casting with true array types
    case T_CAST:
    {
        struct TACOperand expressionResult;

        // Walk the right child of the cast, the subexpression we are casting
        walk_sub_expression(tree->child->sibling, block, scope, TACIndex, tempNum, &expressionResult);
        // set the result's cast as type based on the child of the cast, the type we are casting to
        walk_type_name(tree->child, scope, &expressionResult.castAsType);

        // TODO: allow casting to arrays?
        if (type_is_object(&expressionResult.castAsType))
        {
            char *castToType = type_get_name(&expressionResult.castAsType);
            log_tree(LOG_FATAL, tree->child, "Casting to an object (%s) is not allowed!", castToType);
        }

        struct Type *castFrom = &expressionResult.type;
        struct Type *castTo = &expressionResult.castAsType;

        // If necessary, lop bits off the big end of the value with an explicit bitwise and operation, storing to an intermediate temp
        if (type_compare_allow_implicit_widening(castFrom, castTo) && (castTo->pointerLevel == 0))
        {
            struct TACLine *castBitManipulation = new_tac_line(TT_BITWISE_AND, tree);

            // RHS of the assignment is whatever we are storing, what is being cast
            castBitManipulation->operands[1] = expressionResult;

            // construct the bit pattern we will use in order to properly mask off the extra bits (TODO: will not hold for unsigned types)
            castBitManipulation->operands[2].permutation = VP_LITERAL;
            castBitManipulation->operands[2].type.basicType = VT_U32;

            char literalAndValue[sprintedNumberLength];
            // manually generate a string with an 'F' hex digit for each 4 bits in the mask
            sprintf(literalAndValue, "0x");
            const u8 BITS_PER_BYTE = 8; // TODO: move to substratum_defs?
            size_t maskBitWidth = (BITS_PER_BYTE * type_get_size(tac_get_type_of_operand(castBitManipulation, 1), scope));
            size_t maskBit = 0;
            for (maskBit = 0; maskBit < maskBitWidth; maskBit += 4)
            {
                literalAndValue[2 + (maskBit / 4)] = 'F';
                literalAndValue[3 + (maskBit / 4)] = '\0';
            }

            castBitManipulation->operands[2].name.str = dictionary_lookup_or_insert(parseDict, literalAndValue);

            // destination of our bit manipulation is a temporary variable with the type to which we are casting
            populate_tac_operand_as_temp(&castBitManipulation->operands[0], tempNum);
            castBitManipulation->operands[0].type = *tac_get_type_of_operand(castBitManipulation, 1);

            // attach our bit manipulation operation to the end of the basic block
            basic_block_append(block, castBitManipulation, TACIndex);
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

    case T_SIZEOF:
        walk_sizeof(tree, block, scope, destinationOperand);
        break;

    default:
        log_tree(LOG_FATAL, tree, "Incorrect AST type (%s) seen while linearizing subexpression!", get_token_name(tree->type));
        break;
    }
}

void check_function_return_use(struct AST *tree,
                            struct TACOperand *destinationOperand,
                            struct FunctionEntry *calledFunction)
{
    if ((destinationOperand != NULL) &&
        (calledFunction->returnType.basicType == VT_NULL))
    {
        log_tree(LOG_FATAL, tree, "Attempt to use return value of function %s which does not return anything!", calledFunction->name);
    }
}

struct Stack *walk_argument_pushes(struct AST *argumentRunner,
                                 struct FunctionEntry *calledFunction,
                                 struct BasicBlock *block,
                                 struct Scope *scope,
                                 size_t *TACIndex,
                                 size_t *tempNum,
                                 struct TACOperand *destinationOperand)
{
    log(LOG_DEBUG, "WalkArgumentPushes");

    u8 argumentNumOffset = 0;
    if (calledFunction->isMethod)
    {
        log(LOG_DEBUG, "%s is a method - increment argnumoffset", calledFunction->name);

        argumentNumOffset++;
    }
    if (type_is_object(&calledFunction->returnType))
    {
        log(LOG_DEBUG, "%s is returns an object - increment argnumoffset", calledFunction->name);
        argumentNumOffset++;
    }

    // save first argument so we can generate meaningful error messages if we mismatch argument count
    struct AST *lastArgument = argumentRunner;
    struct Stack *argumentPushes = stack_new();

    struct Stack *argumentTrees = stack_new();
    while (argumentRunner != NULL)
    {
        stack_push(argumentTrees, argumentRunner);
        lastArgument = argumentRunner;
        argumentRunner = argumentRunner->sibling;
    }

    if (argumentTrees->size != (calledFunction->arguments->size - argumentNumOffset))
    {
        log_tree(LOG_FATAL, lastArgument,
                "Error in call to function %s - expected %zu arguments, saw %zu!",
                calledFunction->name,
                calledFunction->arguments->size,
                argumentTrees->size);
    }

    size_t argIndex = calledFunction->arguments->size - 1;
    while (argumentTrees->size > 0)
    {
        struct AST *pushedArgument = stack_pop(argumentTrees);
        struct TACLine *push = new_tac_line(TT_ARG_STORE, pushedArgument);
        stack_push(argumentPushes, push);
        walk_sub_expression(pushedArgument, block, scope, TACIndex, tempNum, &push->operands[0]);

        struct VariableEntry *expectedArgument = calledFunction->arguments->data[argIndex];

        if (type_compare_allow_implicit_widening(tac_get_type_of_operand(push, 0), &expectedArgument->type))
        {
            log(LOG_WARNING, "tacline from %s:%d @ %zu", push->allocFile, push->allocLine, *TACIndex);
            log_tree(LOG_FATAL, pushedArgument,
                    "Error in argument %s passed to function %s!\n\tExpected %s, got %s",
                    expectedArgument->name,
                    calledFunction->name,
                    type_get_name(&expectedArgument->type),
                    type_get_name(tac_get_type_of_operand(push, 0)));
        }

        struct TACOperand decayed;
        copy_tac_operand_decay_arrays(&decayed, &push->operands[0]);

        // allow us to automatically widen
        if (type_get_size(tac_operand_get_type(&decayed), scope) <= type_get_size(&expectedArgument->type, scope))
        {
            push->operands[0].castAsType = expectedArgument->type;
        }
        else
        {
            char *convertFromType = type_get_name(&push->operands[0].type);
            char *convertToType = type_get_name(&expectedArgument->type);
            log_tree(LOG_FATAL, pushedArgument,
                    "Potential narrowing conversion passed to argument %s of function %s\n\tConversion from %s to %s",
                    expectedArgument->name,
                    calledFunction->name,
                    convertFromType,
                    convertToType);
        }

        push->operands[1].name.val = expectedArgument->stackOffset;
        push->operands[1].type.basicType = VT_U64;
        push->operands[1].permutation = VP_LITERAL;

        argIndex--;
    }
    stack_free(argumentTrees);

    return argumentPushes;
}

void handle_struct_return(struct AST *callTree,
                        struct FunctionEntry *calledFunction,
                        struct BasicBlock *block,
                        struct Scope *scope,
                        size_t *TACIndex,
                        size_t *tempNum,
                        struct Stack *argumentPushes,
                        struct TACOperand *destinationOperand)
{
    if (!type_is_object(&calledFunction->returnType))
    {
        return;
    }

    log(LOG_DEBUG, "handleStructReturn for called function %s", calledFunction->name);

    struct TACLine *outPointerPush = new_tac_line(TT_ARG_STORE, callTree);
    // if we actually use the return value of the function
    if (destinationOperand != NULL)
    {
        struct TACOperand intermediateReturnObject;
        populate_tac_operand_as_temp(&intermediateReturnObject, tempNum);
        log_tree(LOG_DEBUG, callTree, "Call to %s returns struct in %s", calledFunction->name, intermediateReturnObject.name.str);
        intermediateReturnObject.type = calledFunction->returnType;
        type_init(&intermediateReturnObject.castAsType);

        *destinationOperand = intermediateReturnObject;
        struct TACOperand *addrOfReturnObject = get_addr_of_operand(callTree, block, scope, TACIndex, tempNum, &intermediateReturnObject);

        copy_tac_operand_decay_arrays(&outPointerPush->operands[0], addrOfReturnObject);
    }
    else
    {
        log(LOG_FATAL, "Unused return value for function %s returning %s", calledFunction->name, type_get_name(&calledFunction->returnType));
    }
    struct VariableEntry *expectedArgument = calledFunction->arguments->data[0];
    outPointerPush->operands[1].name.val = expectedArgument->stackOffset;
    outPointerPush->operands[1].type.basicType = VT_U64;
    outPointerPush->operands[1].permutation = VP_LITERAL;

    stack_push(argumentPushes, outPointerPush);
}

void reserve_and_store_stack_args(struct AST *callTree, struct FunctionEntry *calledFunction, struct Stack *argumentPushes, struct BasicBlock *block, size_t *TACIndex)
{
    log_tree(LOG_DEBUG, callTree, "reserveAndStoreStackArgs");

    while (argumentPushes->size > 0)
    {
        struct TACLine *push = stack_pop(argumentPushes);
        basic_block_append(block, push, TACIndex);
    }
}

struct TACLine *generate_call_tac(struct AST *callTree,
                                struct FunctionEntry *calledFunction,
                                struct BasicBlock *block,
                                size_t *TACIndex,
                                size_t *tempNum,
                                struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, callTree, "generateCallTac");

    struct TACLine *call = new_tac_line(TT_FUNCTION_CALL, callTree);
    call->operands[1].name.str = calledFunction->name;
    basic_block_append(block, call, TACIndex);

    if ((destinationOperand != NULL) && !type_is_object(&calledFunction->returnType))
    {
        call->operands[0].type = calledFunction->returnType;
        populate_tac_operand_as_temp(&call->operands[0], tempNum);

        *destinationOperand = call->operands[0];
    }

    return call;
}

void walk_function_call(struct AST *tree,
                      struct BasicBlock *block,
                      struct Scope *scope,
                      size_t *TACIndex,
                      size_t *tempNum,
                      struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_function_call");

    if (tree->type != T_FUNCTION_CALL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_function_call!", get_token_name(tree->type));
    }

    scope->parentFunction->callsOtherFunction = 1;

    struct FunctionEntry *calledFunction = lookup_fun(scope, tree->child);

    check_function_return_use(tree, destinationOperand, calledFunction);

    struct Stack *argumentPushes = walk_argument_pushes(tree->child->sibling,
                                                      calledFunction,
                                                      block,
                                                      scope,
                                                      TACIndex,
                                                      tempNum,
                                                      destinationOperand);

    handle_struct_return(tree, calledFunction, block, scope, TACIndex, tempNum, argumentPushes, destinationOperand);

    reserve_and_store_stack_args(tree, calledFunction, argumentPushes, block, TACIndex);

    stack_free(argumentPushes);

    generate_call_tac(tree, calledFunction, block, TACIndex, tempNum, destinationOperand);
}

void walk_method_call(struct AST *tree,
                    struct BasicBlock *block,
                    struct Scope *scope,
                    size_t *TACIndex,
                    size_t *tempNum,
                    struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_method_call");

    if (tree->type != T_METHOD_CALL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_method_call!", get_token_name(tree->type));
    }

    scope->parentFunction->callsOtherFunction = 1;

    // don't need to track scope->parentFunction->callsOtherFunction as walk_function_call will do this on our behalf
    struct AST *structTree = tree->child->child;
    struct StructEntry *structCalledOn = NULL;
    struct AST *callTree = tree->child->child->sibling;

    struct TACOperand structOperand;
    memset(&structOperand, 0, sizeof(struct TACOperand));

    switch (structTree->type)
    {
        // if we have struct.member.method() make sure we convert the struct.member load to an LEA
    case T_DOT:
    {
        struct TACLine *memberAccessLine = walk_member_access(structTree, block, scope, TACIndex, tempNum, &structOperand, 0);
        convert_load_to_lea(memberAccessLine, &structOperand);
    }
    break;

    default:
        walk_sub_expression(structTree, block, scope, TACIndex, tempNum, &structOperand);
        if (tac_operand_get_type(&structOperand)->basicType != VT_STRUCT)
        {
            char *nonStructType = type_get_name(tac_operand_get_type(&structOperand));
            log_tree(LOG_FATAL, structTree, "Attempt to call method %s on non-struct type %s", callTree->child->value, nonStructType);
        }
        break;
    }
    structCalledOn = lookup_struct_by_type(scope, tac_operand_get_type(&structOperand));

    struct FunctionEntry *calledFunction = looup_method(structCalledOn, callTree->child, scope);

    check_function_return_use(tree, destinationOperand, calledFunction);

    struct Stack *argumentPushes = walk_argument_pushes(tree->child->child->sibling->child->sibling,
                                                      calledFunction,
                                                      block,
                                                      scope,
                                                      TACIndex,
                                                      tempNum,
                                                      destinationOperand);

    if (tac_operand_get_type(&structOperand)->basicType == VT_ARRAY)
    {
        char *nonDottableType = type_get_name(tac_operand_get_type(&structOperand));
        log_tree(LOG_FATAL, callTree, "Attempt to call method %s on non-dottable type %s", calledFunction->name, nonDottableType);
    }

    // if struct we are calling method on is not indirect, automagically insert an intermediate address-of
    if (tac_operand_get_type(&structOperand)->pointerLevel == 0)
    {
        structOperand = *get_addr_of_operand(tree, block, scope, TACIndex, tempNum, &structOperand);
    }

    struct TACLine *pThisPush = new_tac_line(TT_ARG_STORE, structTree);
    pThisPush->operands[0] = structOperand;
    pThisPush->operands[1].name.val = 0;
    pThisPush->operands[1].type.basicType = VT_U64;
    pThisPush->operands[1].permutation = VP_LITERAL;

    stack_push(argumentPushes, pThisPush);

    handle_struct_return(tree, calledFunction, block, scope, TACIndex, tempNum, argumentPushes, destinationOperand);

    reserve_and_store_stack_args(tree, calledFunction, argumentPushes, block, TACIndex);

    stack_free(argumentPushes);

    struct TACLine *callLine = generate_call_tac(tree, calledFunction, block, TACIndex, tempNum, destinationOperand);
    callLine->operation = TT_METHOD_CALL;
    callLine->operands[2].type.basicType = VT_STRUCT;
    callLine->operands[2].type.nonArray.complexType.name = structCalledOn->name;
}

void walk_associated_call(struct AST *tree,
                        struct BasicBlock *block,
                        struct Scope *scope,
                        size_t *TACIndex,
                        size_t *tempNum,
                        struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_associated_call");

    if (tree->type != T_ASSOCIATED_CALL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to T_ASSOCIATED_CALL!", get_token_name(tree->type));
    }

    scope->parentFunction->callsOtherFunction = 1;

    // don't need to track scope->parentFunction->callsOtherFunction as walk_function_call will do this on our behalf
    struct AST *structTypeTree = tree->child->child;
    struct StructEntry *structCalledOn = NULL;
    struct AST *callTree = tree->child->sibling;

    if (structTypeTree->type != T_IDENTIFIER)
    {
        InternalError("Malformed AST in walk_associated_call - expected identifier on LHS but got %s instead", get_token_name(structTypeTree->type));
    }
    structCalledOn = lookup_struct(scope, structTypeTree);

    struct FunctionEntry *calledFunction = lookup_associated_function(structCalledOn, callTree->child, scope);

    check_function_return_use(tree, destinationOperand, calledFunction);

    struct Stack *argumentPushes = walk_argument_pushes(tree->child->sibling->child->sibling,
                                                      calledFunction,
                                                      block,
                                                      scope,
                                                      TACIndex,
                                                      tempNum,
                                                      destinationOperand);
    handle_struct_return(tree, calledFunction, block, scope, TACIndex, tempNum, argumentPushes, destinationOperand);

    reserve_and_store_stack_args(tree, calledFunction, argumentPushes, block, TACIndex);

    stack_free(argumentPushes);

    struct TACLine *callLine = generate_call_tac(tree, calledFunction, block, TACIndex, tempNum, destinationOperand);
    callLine->operation = TT_ASSOCIATED_CALL;
    callLine->operands[2].type.basicType = VT_STRUCT;
    callLine->operands[2].type.nonArray.complexType.name = structCalledOn->name;
}

struct TACLine *walk_member_access(struct AST *tree,
                                 struct BasicBlock *block,
                                 struct Scope *scope,
                                 size_t *TACIndex,
                                 size_t *tempNum,
                                 struct TACOperand *srcDestOperand,
                                 size_t depth)
{
    log_tree(LOG_DEBUG, tree, "walk_member_access");

    if (tree->type != T_DOT)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_member_access!", get_token_name(tree->type));
    }

    struct AST *lhs = tree->child;
    struct AST *rhs = lhs->sibling;

    if (rhs->type != T_IDENTIFIER)
    {
        log_tree(LOG_FATAL, rhs,
                "Expected identifier on RHS of %s operator, got %s (%s) instead!",
                get_token_name(tree->type),
                rhs->value,
                get_token_name(rhs->type));
    }

    struct TACLine *accessLine = NULL;

    switch (lhs->type)
    {
    // in the case that we are dot-ing a LHS which is a dot, walk_member_access will generate the TAC line for the *read* which gets us the address to dot on
    case T_DOT:
        accessLine = walk_member_access(lhs, block, scope, TACIndex, tempNum, srcDestOperand, depth + 1);
        break;

    // for all other cases, we can  populate the LHS using walk_sub_expression as it is just a more basic read
    default:
    {
        // the LHS of the dot is the struct instance being accessed
        struct AST *structTree = tree->child;
        // the RHS is what member we are accessing
        struct AST *member = tree->child->sibling;

        // TODO: check more deeply what's being dotted? Shortlist: dereference, array index, identifier, maybe some pointer arithmetic?
        //		 	- things like (myObjectPointer & 0xFFFFFFF0)->member are obviously wrong, so probably should disallow
        // prevent silly things like (&a)->b

        if (member->type != T_IDENTIFIER)
        {
            log_tree(LOG_FATAL, member,
                    "Expected identifier on RHS of dot operator, got %s (%s) instead!",
                    member->value,
                    get_token_name(member->type));
        }

        // our access line is a completely new TAC line, which is a load operation with an offset, storing the load result to a temp
        accessLine = new_tac_line(TT_LOAD_OFF, tree);

        populate_tac_operand_as_temp(&accessLine->operands[0], tempNum);

        // we may need to do some manipulation of the subexpression depending on what exactly we're dotting
        switch (structTree->type)
        {
        case T_DEREFERENCE:
        {
            // let walk_dereference do the heavy lifting for us
            struct TACOperand *dereferencedOperand = walk_dereference(structTree, block, scope, TACIndex, tempNum);

            // make sure we are generally dotting something sane
            struct Type *accessedType = tac_operand_get_type(dereferencedOperand);

            check_accessed_struct_for_dot(structTree, scope, accessedType);
            // additional check so that if we dereference a struct single-pointer we force not putting the dereference there
            if (accessedType->pointerLevel == 0)
            {
                char *dereferencedTypeName = type_get_name(accessedType);
                log_tree(LOG_FATAL, structTree, "Use of dereference on single-indirect type %s before dot '(*struct).member' is prohibited - just use 'struct.member' instead", dereferencedTypeName);
            }

            copy_tac_operand_decay_arrays(&accessLine->operands[1], dereferencedOperand);
        }
        break;

        case T_ARRAY_INDEX:
        {
            // let walk_array_ref do the heavy lifting for us
            struct TACLine *arrayRefToDot = walk_array_ref(structTree, block, scope, TACIndex, tempNum);

            // before we convert our array ref to an LEA to get the address of the struct we're dotting, check to make sure everything is good
            check_accessed_struct_for_dot(tree, scope, tac_get_type_of_operand(arrayRefToDot, 0));

            // now that we know we are dotting something valid, we will just use the array reference as an address calculation for the base of whatever we're dotting
            convert_load_to_lea(arrayRefToDot, &accessLine->operands[1]);
        }
        break;

        case T_FUNCTION_CALL:
        {
            walk_function_call(structTree, block, scope, TACIndex, tempNum, &accessLine->operands[1]);
        }
        break;

        case T_METHOD_CALL:
        {
            walk_method_call(structTree, block, scope, TACIndex, tempNum, &accessLine->operands[1]);
        }
        break;

        case T_SELF:
        case T_IDENTIFIER:
        {
            // if we are dotting an identifier, insert an address-of if it is not a pointer already
            struct VariableEntry *dottedVariable = lookup_var(scope, structTree);

            if (dottedVariable->type.pointerLevel == 0)
            {
                struct TACOperand dottedOperand;
                memset(&dottedOperand, 0, sizeof(struct TACOperand));

                walk_sub_expression(structTree, block, scope, TACIndex, tempNum, &dottedOperand);

                if (dottedOperand.permutation != VP_TEMP)
                {
                    // while this check is duplicated in the checks immediately following the switch,
                    // we may be able to print more verbose error info if we are directly member-accessing an identifier, so do it here.
                    check_accessed_struct_for_dot(structTree, scope, tac_operand_get_type(&dottedOperand));
                }

                struct TACOperand *addrOfDottedVariable = get_addr_of_operand(structTree, block, scope, TACIndex, tempNum, &dottedOperand);
                copy_tac_operand_decay_arrays(&accessLine->operands[1], addrOfDottedVariable);
            }
            else
            {
                walk_sub_expression(structTree, block, scope, TACIndex, tempNum, &accessLine->operands[1]);
            }
        }
        break;

        default:
            log_tree(LOG_FATAL, structTree, "Dot operator member access on disallowed tree type %s", get_token_name(structTree->type));
            break;
        }

        accessLine->operands[2].type.basicType = VT_U32;
        accessLine->operands[2].permutation = VP_LITERAL;
    }
    break;
    }

    struct Type *accessedType = tac_get_type_of_operand(accessLine, 1);
    if (accessedType->basicType != VT_STRUCT)
    {
        char *accessedTypeName = type_get_name(accessedType);
        log_tree(LOG_FATAL, tree, "Use of dot operator for member access on non-struct type %s", accessedTypeName);
    }

    // get the StructEntry and StructMemberOffset of what we're accessing within and the member we access
    struct StructEntry *accessedStruct = lookup_struct_by_type(scope, accessedType);
    struct StructMemberOffset *accessedMember = lookup_member_variable(accessedStruct, rhs, scope);

    // populate type information (use cast for the first operand as we are treating a struct as a pointer to something else with a given offset)
    accessLine->operands[1].castAsType = accessedMember->variable->type;
    accessLine->operands[0].type = *tac_get_type_of_operand(accessLine, 1); // copy type info to the temp we're reading to
    *tac_get_type_of_operand(accessLine, 0) = *tac_get_type_of_operand(accessLine, 0);

    accessLine->operands[2].name.val += accessedMember->offset;

    if (depth == 0)
    {
        basic_block_append(block, accessLine, TACIndex);
        *srcDestOperand = accessLine->operands[0];
    }

    return accessLine;
}

void walk_non_pointer_arithmetic(struct AST *tree,
                              struct BasicBlock *block,
                              struct Scope *scope,
                              size_t *TACIndex,
                              size_t *tempNum,
                              struct TACLine *expression)
{
    log_tree(LOG_DEBUG, tree, "WalkNonPointerArithmetic");

    switch (tree->type)
    {
    case T_MULTIPLY:
        expression->reorderable = 1;
        expression->operation = TT_MUL;
        break;

    case T_BITWISE_AND:
        expression->reorderable = 1;
        expression->operation = TT_BITWISE_AND;
        break;

    case T_BITWISE_OR:
        expression->reorderable = 1;
        expression->operation = TT_BITWISE_OR;
        break;

    case T_BITWISE_XOR:
        expression->reorderable = 1;
        expression->operation = TT_BITWISE_XOR;
        break;

    case T_BITWISE_NOT:
        expression->reorderable = 1;
        expression->operation = TT_BITWISE_NOT;
        break;

    case T_DIVIDE:
        expression->operation = TT_DIV;
        break;

    case T_MODULO:
        expression->operation = TT_MODULO;
        break;

    case T_LSHIFT:
        expression->operation = TT_LSHIFT;
        break;

    case T_RSHIFT:
        expression->operation = TT_RSHIFT;
        break;

    default:
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to WalkNonPointerArithmetic!", get_token_name(tree->type));
        break;
    }

    walk_sub_expression(tree->child, block, scope, TACIndex, tempNum, &expression->operands[1]);
    walk_sub_expression(tree->child->sibling, block, scope, TACIndex, tempNum, &expression->operands[2]);

    for (u8 operandIndex = 1; operandIndex < 2; operandIndex++)
    {
        struct Type *checkedType = tac_get_type_of_operand(expression, operandIndex);
        if ((checkedType->pointerLevel > 0) || (checkedType->basicType == VT_ARRAY))
        {
            char *typeName = type_get_name(checkedType);
            log_tree(LOG_FATAL, tree->child, "Arithmetic operation attempted on type %s, %s is only allowed on non-indirect types", typeName, tree->value);
        }
    }

    if (type_get_size(tac_get_type_of_operand(expression, 1), scope) > type_get_size(tac_get_type_of_operand(expression, 2), scope))
    {
        *tac_get_type_of_operand(expression, 0) = *tac_get_type_of_operand(expression, 1);
    }
    else
    {
        *tac_get_type_of_operand(expression, 0) = *tac_get_type_of_operand(expression, 2);
    }
}

struct TACOperand *walk_expression(struct AST *tree,
                                  struct BasicBlock *block,
                                  struct Scope *scope,
                                  size_t *TACIndex,
                                  size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_expression");

    // generically set to TT_ADD, we will actually set the operation within switch cases
    struct TACLine *expression = new_tac_line(TT_SUBTRACT, tree);

    populate_tac_operand_as_temp(&expression->operands[0], tempNum);

    u8 fallingThrough = 0;

    switch (tree->type)
    {
    // basic arithmetic
    case T_MULTIPLY:
    case T_BITWISE_AND:
    case T_BITWISE_OR:
    case T_BITWISE_XOR:
    case T_BITWISE_NOT:
    case T_DIVIDE:
    case T_MODULO:
    case T_LSHIFT:
    case T_RSHIFT:
        walk_non_pointer_arithmetic(tree, block, scope, TACIndex, tempNum, expression);
        break;

    case T_ADD:
        expression->operation = TT_ADD;
        expression->reorderable = 1;
        fallingThrough = 1;
    case T_SUBTRACT:
    {
        if (!fallingThrough)
        {
            expression->operation = TT_SUBTRACT;
            fallingThrough = 1;
        }

        walk_sub_expression(tree->child, block, scope, TACIndex, tempNum, &expression->operands[1]);

        // TODO: also scale arithmetic on array types
        if (tac_get_type_of_operand(expression, 1)->pointerLevel > 0)
        {
            struct TACLine *scaleMultiply = set_up_scale_multiplication(tree, scope, TACIndex, tempNum, tac_get_type_of_operand(expression, 1));
            walk_sub_expression(tree->child->sibling, block, scope, TACIndex, tempNum, &scaleMultiply->operands[1]);

            scaleMultiply->operands[0].type = scaleMultiply->operands[1].type;
            copy_tac_operand_decay_arrays(&expression->operands[2], &scaleMultiply->operands[0]);

            basic_block_append(block, scaleMultiply, TACIndex);
        }
        else
        {
            walk_sub_expression(tree->child->sibling, block, scope, TACIndex, tempNum, &expression->operands[2]);
        }

        // TODO: generate errors for array types
        struct TACOperand *operandA = &expression->operands[1];
        struct TACOperand *operandB = &expression->operands[2];
        if ((operandA->type.pointerLevel > 0) && (operandB->type.pointerLevel > 0))
        {
            log_tree(LOG_FATAL, tree, "Arithmetic between 2 pointers is not allowed!");
        }

        // TODO generate errors for bad pointer arithmetic here
        if (type_get_size(tac_operand_get_type(operandA), scope) > type_get_size(tac_operand_get_type(operandB), scope))
        {
            *tac_get_type_of_operand(expression, 0) = *tac_operand_get_type(operandA);
        }
        else
        {
            *tac_get_type_of_operand(expression, 0) = *tac_operand_get_type(operandB);
        }
    }
    break;

    default:
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_expression!", get_token_name(tree->type));
    }

    basic_block_append(block, expression, TACIndex);

    return &expression->operands[0];
}

struct TACLine *walk_array_ref(struct AST *tree,
                             struct BasicBlock *block,
                             struct Scope *scope,
                             size_t *TACIndex,
                             size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_array_ref");

    if (tree->type != T_ARRAY_INDEX)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_array_ref!", get_token_name(tree->type));
    }

    struct AST *arrayBase = tree->child;
    struct AST *arrayIndex = tree->child->sibling;

    struct TACLine *arrayRefTac = new_tac_line(TT_LOAD_ARR, tree);
    struct Type *arrayBaseType = NULL;

    switch (arrayBase->type)
    {
    // if the array base is an identifier, we can just look it up
    case T_IDENTIFIER:
    {
        struct VariableEntry *arrayVariable = lookup_var(scope, arrayBase);
        populate_tac_operand_from_variable(&arrayRefTac->operands[1], arrayVariable);
        arrayBaseType = tac_get_type_of_operand(arrayRefTac, 1);

        // sanity check - can print the name of the variable if incorrectly accessing an identifier
        // TODO: check against size of array if index is constant?
        if ((arrayBaseType->pointerLevel == 0) && (arrayBaseType->basicType != VT_ARRAY))
        {
            log_tree(LOG_FATAL, arrayBase, "Array reference on non-indirect variable %s %s", type_get_name(arrayBaseType), arrayBase->value);
        }
    }
    break;

    case T_DOT:
    {
        struct TACLine *arrayBaseAccessLine = walk_member_access(arrayBase, block, scope, TACIndex, tempNum, &arrayRefTac->operands[1], 0);
        convert_load_to_lea(arrayBaseAccessLine, &arrayRefTac->operands[1]);
        arrayBaseType = tac_get_type_of_operand(arrayBaseAccessLine, 0);
    }
    break;

    // otherwise, we need to Walk the subexpression to get the array base
    default:
    {
        walk_sub_expression(arrayBase, block, scope, TACIndex, tempNum, &arrayRefTac->operands[1]);
        arrayBaseType = tac_get_type_of_operand(arrayRefTac, 1);

        // sanity check - can only print the type of the base if incorrectly accessing a non-identifier through a subexpression
        if ((arrayBaseType->pointerLevel == 0) && (arrayBaseType->basicType != VT_ARRAY))
        {
            log_tree(LOG_FATAL, arrayBase, "Array reference on non-indirect type %s", type_get_name(arrayBaseType));
        }
    }
    break;
    }

    copy_tac_operand_decay_arrays(&arrayRefTac->operands[0], &arrayRefTac->operands[1]);
    populate_tac_operand_as_temp(&arrayRefTac->operands[0], tempNum);

    type_single_decay(&arrayRefTac->operands[0].type);
    if (arrayRefTac->operands[0].type.pointerLevel < 1)
    {
        InternalError("Result of decay on array-referenced type has non-indirect type of %s", type_get_name(tac_get_type_of_operand(arrayRefTac, 0)));
    }
    arrayRefTac->operands[0].type.pointerLevel--;
    if (arrayIndex->type == T_CONSTANT)
    {
        // if referencing an array of structs, implicitly convert to an LEA to avoid copying the entire struct to a temp
        if (type_is_struct_object(arrayBaseType))
        {
            arrayRefTac->operation = TT_LEA_OFF;
            arrayRefTac->operands[0].type.pointerLevel++;
        }
        else
        {
            arrayRefTac->operation = TT_LOAD_OFF;
        }

        // TODO: abstract this
        int indexSize = atoi(arrayIndex->value);
        indexSize *= 1 << align_size(type_get_size_of_array_element(arrayBaseType, scope));

        arrayRefTac->operands[2].name.val = indexSize;
        arrayRefTac->operands[2].permutation = VP_LITERAL;
        arrayRefTac->operands[2].type.basicType = select_variable_type_for_number(arrayRefTac->operands[2].name.val);
    }
    // otherwise, the index is either a variable or subexpression
    else
    {
        // if referencing a struct, implicitly convert to an LEA to avoid copying the entire struct to a temp
        if (type_is_struct_object(arrayBaseType))
        {
            arrayRefTac->operation = TT_LEA_ARR;
            arrayRefTac->operands[0].type.pointerLevel++;
        }
        // set the scale for the array access

        arrayRefTac->operands[3].name.val = align_size(type_get_size_of_array_element(arrayBaseType, scope));
        arrayRefTac->operands[3].permutation = VP_LITERAL;
        arrayRefTac->operands[3].type.basicType = select_variable_type_for_number(arrayRefTac->operands[3].name.val);

        walk_sub_expression(arrayIndex, block, scope, TACIndex, tempNum, &arrayRefTac->operands[2]);
    }

    basic_block_append(block, arrayRefTac, TACIndex);
    return arrayRefTac;
}

struct TACOperand *walk_dereference(struct AST *tree,
                                   struct BasicBlock *block,
                                   struct Scope *scope,
                                   size_t *TACIndex,
                                   size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_dereference");

    if (tree->type != T_DEREFERENCE)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_dereference!", get_token_name(tree->type));
    }

    struct TACLine *dereference = new_tac_line(TT_LOAD, tree);

    switch (tree->child->type)
    {
    case T_ADD:
    case T_SUBTRACT:
    {
        walk_pointer_arithmetic(tree->child, block, scope, TACIndex, tempNum, &dereference->operands[1]);
    }
    break;

    default:
        walk_sub_expression(tree->child, block, scope, TACIndex, tempNum, &dereference->operands[1]);
        break;
    }

    copy_tac_operand_decay_arrays(&dereference->operands[0], &dereference->operands[1]);
    tac_get_type_of_operand(dereference, 0)->pointerLevel--;
    populate_tac_operand_as_temp(&dereference->operands[0], tempNum);

    basic_block_append(block, dereference, TACIndex);

    return &dereference->operands[0];
}

struct TACOperand *walk_addr_of(struct AST *tree,
                              struct BasicBlock *block,
                              struct Scope *scope,
                              size_t *TACIndex,
                              size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_addr_of");

    if (tree->type != T_ADDRESS_OF)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to WalkAddressOf!", get_token_name(tree->type));
    }

    // TODO: helper function for getting address of
    struct TACLine *addrOfLine = new_tac_line(TT_ADDROF, tree);
    populate_tac_operand_as_temp(&addrOfLine->operands[0], tempNum);

    switch (tree->child->type)
    {
    // look up the variable entry and ensure that we will spill it to the stack since we take its address
    case T_IDENTIFIER:
    {
        struct VariableEntry *addrTakenOf = lookup_var(scope, tree->child);
        if (addrTakenOf->type.basicType == VT_ARRAY)
        {
            log_tree(LOG_FATAL, tree->child, "Can't take address of local array %s!", addrTakenOf->name);
        }
        addrTakenOf->mustSpill = 1;
        walk_sub_expression(tree->child, block, scope, TACIndex, tempNum, &addrOfLine->operands[1]);
    }
    break;

    case T_ARRAY_INDEX:
    {
        // use walk_array_ref to generate the access we need, just the direct accessing load to an lea to calculate the address we would have loaded from
        struct TACLine *arrayRefLine = walk_array_ref(tree->child, block, scope, TACIndex, tempNum);
        convert_load_to_lea(arrayRefLine, NULL);

        // early return, no need for explicit address-of TAC
        free_tac(addrOfLine);
        addrOfLine = NULL;

        return &arrayRefLine->operands[0];
    }
    break;

    case T_DOT:
    {
        // walk_member_access can do everything we need
        // the only thing we have to do is ensure we have an LEA at the end instead of a direct read in the case of the dot operator
        struct TACLine *memberAccessLine = walk_member_access(tree->child, block, scope, TACIndex, tempNum, &addrOfLine->operands[1], 0);
        convert_load_to_lea(memberAccessLine, &addrOfLine->operands[1]);

        // free the line created at the top of this function and return early
        free_tac(addrOfLine);
        return &memberAccessLine->operands[0];
    }
    break;

    default:
        log_tree(LOG_FATAL, tree, "Address of operator is not supported for non-identifiers! Saw %s", get_token_name(tree->child->type));
    }

    addrOfLine->operands[0].type = *tac_get_type_of_operand(addrOfLine, 1);
    addrOfLine->operands[0].type.pointerLevel++;

    basic_block_append(block, addrOfLine, TACIndex);

    return &addrOfLine->operands[0];
}

void walk_pointer_arithmetic(struct AST *tree,
                           struct BasicBlock *block,
                           struct Scope *scope,
                           size_t *TACIndex,
                           size_t *tempNum,
                           struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_pointer_arithmetic");

    if ((tree->type != T_ADD) && (tree->type != T_SUBTRACT))
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_pointer_arithmetic!", get_token_name(tree->type));
    }

    struct AST *pointerArithLhs = tree->child;
    struct AST *pointerArithRhs = tree->child->sibling;

    struct TACLine *pointerArithmetic = new_tac_line(TT_ADD, tree->child);
    if (tree->type == T_SUBTRACT)
    {
        pointerArithmetic->operation = TT_SUBTRACT;
    }

    walk_sub_expression(pointerArithLhs, block, scope, TACIndex, tempNum, &pointerArithmetic->operands[1]);

    populate_tac_operand_as_temp(&pointerArithmetic->operands[0], tempNum);
    copy_tac_operand_decay_arrays(&pointerArithmetic->operands[0], &pointerArithmetic->operands[1]);

    struct TACLine *scaleMultiplication = set_up_scale_multiplication(pointerArithRhs,
                                                                   scope,
                                                                   TACIndex,
                                                                   tempNum,
                                                                   tac_get_type_of_operand(pointerArithmetic, 1));

    walk_sub_expression(pointerArithRhs, block, scope, TACIndex, tempNum, &scaleMultiplication->operands[1]);

    *tac_get_type_of_operand(scaleMultiplication, 0) = *tac_get_type_of_operand(scaleMultiplication, 1);

    copy_tac_operand_decay_arrays(&pointerArithmetic->operands[2], &scaleMultiplication->operands[0]);

    basic_block_append(block, scaleMultiplication, TACIndex);
    basic_block_append(block, pointerArithmetic, TACIndex);

    *destinationOperand = pointerArithmetic->operands[0];
}

void walk_asm_block(struct AST *tree,
                  struct BasicBlock *block,
                  struct Scope *scope,
                  size_t *TACIndex,
                  size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_asm_block");

    if (tree->type != T_ASM)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_asm_block!", get_token_name(tree->type));
    }

    struct AST *asmRunner = tree->child;
    while (asmRunner != NULL)
    {
        if (asmRunner->type != T_ASM)
        {
            log_tree(LOG_FATAL, tree, "Non-asm seen as contents of ASM block!");
        }

        struct TACLine *asmLine = new_tac_line(TT_ASM, asmRunner);
        asmLine->operands[0].name.str = asmRunner->value;

        basic_block_append(block, asmLine, TACIndex);

        asmRunner = asmRunner->sibling;
    }
}

void walk_string_literal(struct AST *tree,
                       struct BasicBlock *block,
                       struct Scope *scope,
                       struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_string_literal");

    if (tree->type != T_STRING_LITERAL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_string_literal!", get_token_name(tree->type));
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
                const u16 CHARS_IN_ALPHABET = 26;
                char altVal = (char)(stringName[charIndex] % (CHARS_IN_ALPHABET * 1));
                if (altVal > (CHARS_IN_ALPHABET - 1))
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
    struct ScopeMember *existingMember = scope_lookup(scope, stringName);

    // if we already have a string literal for this thing, nothing else to do
    if (existingMember == NULL)
    {
        struct AST fakeStringTree;
        fakeStringTree.value = stringName;
        fakeStringTree.sourceFile = tree->sourceFile;
        fakeStringTree.sourceLine = tree->sourceLine;
        fakeStringTree.sourceCol = tree->sourceCol;

        struct Type stringType;
        type_set_basic_type(&stringType, VT_ARRAY, NULL, 0);
        struct Type charType;
        type_init(&charType);
        charType.basicType = VT_U8;
        stringType.array.type = dictionary_lookup_or_insert(typeDict, &charType);
        stringType.array.size = stringLength;

        stringLiteralEntry = create_variable(scope, &fakeStringTree, &stringType, 1, 0, 0, A_PUBLIC);
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
    populate_tac_operand_from_variable(destinationOperand, stringLiteralEntry);
    destinationOperand->name.str = stringName;
    destinationOperand->type = stringLiteralEntry->type;
}

void walk_sizeof(struct AST *tree,
                struct BasicBlock *block,
                struct Scope *scope,
                struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_sizeof");

    if (tree->type != T_SIZEOF)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_sizeof!", get_token_name(tree->type));
    }

    size_t sizeInBytes = 0;

    switch (tree->child->type)
    {
    // if we see an identifier, it may be an identifier or a struct name
    case T_IDENTIFIER:
    {
        // do a generic scope lookup on the identifier
        struct ScopeMember *lookedUpIdentifier = scope_lookup(scope, tree->child->value);

        // if it looks up nothing, or it's a variable
        if ((lookedUpIdentifier == NULL) || (lookedUpIdentifier->type == E_VARIABLE))
        {
            // scope_lookup_var is not redundant as it will give us a 'use of undeclared' error in the case where we looked up nothing
            struct VariableEntry *getSizeof = lookup_var(scope, tree->child);

            sizeInBytes = type_get_size(&getSizeof->type, scope);
        }
        // we looked something up but it's not a variable
        else
        {
            struct StructEntry *getSizeof = lookup_struct(scope, tree->child);

            sizeInBytes = getSizeof->totalSize;
        }
    }
    break;

    case T_TYPE_NAME:
    {
        struct Type getSizeof;
        walk_type_name(tree->child, scope, &getSizeof);

        sizeInBytes = type_get_size(&getSizeof, scope);
    }
    break;
    default:
        log_tree(LOG_FATAL, tree, "sizeof is only supported on type names and identifiers!");
    }

    char sizeString[sprintedNumberLength];
    snprintf(sizeString, sprintedNumberLength - 1, "%zu", sizeInBytes);
    destinationOperand->type.basicType = VT_U8;
    destinationOperand->permutation = VP_LITERAL;
    destinationOperand->name.str = dictionary_lookup_or_insert(parseDict, sizeString);
}