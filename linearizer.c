#include "linearizer.h"
#include "codegen_generic.h"
#include "linearizer_generic.h"
#include "log.h"
#include "symtab.h"

#include <ctype.h>

/*
 TODO: fix TAC index tracking across branches and loops,
 such that for a given branch or loop, the TAC index immediately after is the maximum of the TAC indices of all branches and loops,
 and that each parallel branch has parallel TAC indices
 */

/*
 * These functions Walk the AST and convert it to three-address code
 */
struct TempList *temps;
extern struct Dictionary *parseDict;
const u8 TYPE_DICT_SIZE = 100;
struct SymbolTable *walk_program(struct Ast *program)
{
    struct SymbolTable *programTable = symbol_table_new("Program");
    struct BasicBlock *globalBlock = scope_lookup(programTable->globalScope, "globalblock", E_BASICBLOCK)->entry;
    struct BasicBlock *asmBlock = basic_block_new(1);
    scope_add_basic_block(programTable->globalScope, asmBlock);
    temps = temp_list_new();

    size_t globalTacIndex = 0;
    size_t globalTempNum = 0;

    struct Ast *programRunner = program;
    while (programRunner != NULL)
    {
        switch (programRunner->type)
        {
        case T_VARIABLE_DECLARATION:
            // walk_variable_declaration sets isGlobal for us by checking if there is no parent scope
            walk_variable_declaration(programRunner, programTable->globalScope, &globalTacIndex, &globalTempNum, false);
            break;

        case T_EXTERN:
        {
            struct VariableEntry *declaredVariable = walk_variable_declaration(programRunner->child, programTable->globalScope, &globalTacIndex, &globalTempNum, false);
            declaredVariable->isExtern = 1;
        }
        break;

        case T_STRUCT:
            walk_struct_declaration(programRunner, programTable->globalScope, NULL);
            break;

        case T_ENUM:
            walk_enum_declaration(programRunner, globalBlock, programTable->globalScope, NULL);
            break;

        case T_ASSIGN:
            walk_assignment(programRunner, globalBlock, programTable->globalScope, &globalTacIndex, &globalTempNum);
            break;

        case T_IMPL:
            walk_implementation_block(programRunner, programTable->globalScope);
            break;

        case T_FUN:
            walk_function_declaration(programRunner, programTable->globalScope, NULL, A_PUBLIC, false);
            break;

        case T_GENERIC:
            walk_generic(programRunner, programTable->globalScope);
            break;

        // ignore asm blocks
        case T_ASM:
            walk_asm_block(programRunner, asmBlock, programTable->globalScope, &globalTacIndex, &globalTempNum);
            break;

        case T_TRAIT:
            walk_trait_declaration(programRunner, programTable->globalScope);
            break;

        default:
            InternalError("Malformed AST in WalkProgram: got %s with type %s",
                          programRunner->value,
                          token_get_name(programRunner->type));
            break;
        }
        programRunner = programRunner->sibling;
    }

    return programTable;
}

struct TACOperand *get_addr_of_operand(struct Ast *tree,
                                       struct BasicBlock *block,
                                       struct Scope *scope,
                                       size_t *tacIndex,
                                       size_t *tempNum,
                                       struct TACOperand *getAddrOf)
{
    struct TACLine *addrOfLine = new_tac_line(TT_ADDROF, tree);

    getAddrOf->name.variable->mustSpill = true;
    addrOfLine->operands.addrof.source = *getAddrOf;
    struct TacAddrOf *operands = &addrOfLine->operands.addrof;
    operands->source = *getAddrOf;

    struct Type typeOfAddress = type_duplicate_non_pointer(tac_operand_get_type(getAddrOf));
    typeOfAddress.pointerLevel++;

    tac_operand_populate_as_temp(scope, &operands->destination, tempNum, &typeOfAddress);

    *tac_operand_get_type(&operands->destination) = *tac_operand_get_type(&operands->source);

    tac_operand_get_type(&operands->destination)->pointerLevel++;
    basic_block_append(block, addrOfLine, tacIndex);

    return &operands->destination;
}

void check_any_type_use(struct Type *type, struct Ast *typeTree)
{
    // if declaring something with the 'any' type, make sure it's only as a pointer (as its intended use is to point to unstructured data)
    if (type->basicType == VT_ARRAY || type->basicType == VT_ANY)
    {
        struct Type anyCheckRunner = *type;
        while (anyCheckRunner.basicType == VT_ARRAY)
        {
            anyCheckRunner = *anyCheckRunner.array.type;
        }

        if ((anyCheckRunner.pointerLevel == 0) && (anyCheckRunner.basicType == VT_ANY))
        {
            if (type->basicType == VT_ARRAY)
            {
                log_tree(LOG_FATAL, typeTree, "Use of the type 'any' in arrays is forbidden!\n'any' is meant to represent unstructured data as a pointer type only\n(declare as 'any *', 'any **', etc...)");
            }
            else
            {
                log_tree(LOG_FATAL, typeTree, "Use of the type 'any' without indirection is forbidden!\n'any' is meant to represent unstructured data as a pointer type only\n(declare as 'any *', 'any **', etc...)");
            }
        }
    }
}

struct Type walk_non_pointer_type_name(struct Scope *scope,
                                       struct Ast *tree,
                                       struct TypeEntry *fieldOf)
{
    struct Type wipType = {0};
    type_init(&wipType);
    switch (tree->type)
    {
    case T_ANY:
        wipType.basicType = VT_ANY;
        break;

    case T_U8:
        wipType.basicType = VT_U8;
        break;

    case T_U16:
        wipType.basicType = VT_U16;
        break;

    case T_U32:
        wipType.basicType = VT_U32;
        break;

    case T_U64:
        wipType.basicType = VT_U64;
        break;

    case T_IDENTIFIER:
    {
        while (scope != NULL)
        {
            if (fieldOf == NULL)
            {

                if ((scope->parentFunction != NULL) &&
                    (scope->parentFunction->implementedFor != NULL) &&
                    (scope->parentScope == scope->parentFunction->implementedFor->parentScope) &&
                    (scope->parentFunction->implementedFor->genericType == G_BASE) &&
                    (list_find(scope->parentFunction->implementedFor->generic.base.paramNames, tree->value) != NULL))
                {
                    wipType.basicType = VT_GENERIC_PARAM;
                    wipType.nonArray.complexType.name = tree->value;
                    return wipType;
                }
            }
            else
            {
                if (fieldOf->genericType == G_BASE)
                {
                    if (list_find(fieldOf->generic.base.paramNames, tree->value) != NULL)
                    {
                        wipType.basicType = VT_GENERIC_PARAM;
                        wipType.nonArray.complexType.name = tree->value;
                        return wipType;
                    }
                }
            }

            struct ScopeMember *lookedUp = scope_lookup_no_parent(scope, tree->value, E_TYPE);
            if (lookedUp != NULL)
            {
                struct TypeEntry *lookedUpType = lookedUp->entry;
                wipType = lookedUpType->type;
                break;
            }

            scope = scope->parentScope;
        }

        if (wipType.basicType == VT_NULL)
        {
            log_tree(LOG_FATAL, tree, "%s does not name a type!", tree->value);
        }
    }
    break;

    case T_CAP_SELF:
    {
        wipType.basicType = VT_SELF;
        if ((scope->parentFunction == NULL) || (scope->parentFunction->implementedFor == NULL))
        {
            log_tree(LOG_FATAL, tree, "Use of 'Self' outside of impl scope!");
        }
    }
    break;

    case T_GENERIC_INSTANCE:
    {
        struct TypeEntry *instance = walk_type_name_or_generic_instantiation(scope, tree);
        if (instance->genericType != G_INSTANCE)
        {
            log_tree(LOG_FATAL, tree, "walk_type_name_or_generic_instantiation returned non-generic-instance %s!", instance->baseName);
        }

        wipType = instance->type;
    }
    break;

    default:
        log_tree(LOG_FATAL, tree, "Malformed AST (%s) seen in walk_non_pointer_type_name!", token_get_name(tree->type));
    }

    return wipType;
}

void walk_type_name(struct Ast *tree, struct Scope *scope,
                    struct Type *populateTypeTo,
                    struct TypeEntry *fieldOf)
{
    log_tree(LOG_DEBUG, tree, "WalkTypeName");
    if (tree->type != T_TYPE_NAME)
    {
        InternalError("Wrong AST (%s) passed to WalkTypeName!", token_get_name(tree->type));
    }

    type_init(populateTypeTo);

    *populateTypeTo = walk_non_pointer_type_name(scope, tree->child, fieldOf);

    struct Ast *declaredArray = NULL;
    populateTypeTo->pointerLevel = scrape_pointers(tree->child, &declaredArray);

    check_any_type_use(populateTypeTo, tree->child);

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

        struct Type *arrayedType = type_duplicate(populateTypeTo);

        // TODO: multidimensional array declarations
        populateTypeTo->basicType = VT_ARRAY;
        populateTypeTo->array.size = declaredArraySize;
        populateTypeTo->array.type = arrayedType;
        populateTypeTo->array.initializeArrayTo = NULL;
    }
}

struct VariableEntry *walk_variable_declaration(struct Ast *tree,
                                                struct Scope *scope,
                                                const size_t *tacIndex,
                                                const size_t *tempNum,
                                                u8 isArgument)
{
    log_tree(LOG_DEBUG, tree, "walk_variable_declaration");

    if (tree->type != T_VARIABLE_DECLARATION)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_variable_declaration!", token_get_name(tree->type));
    }

    struct Type declaredType = {0};

    /* 'struct' trees' children are the struct name
     * other variables' children are the pointer or variable name
     * so we need to start at tree->child for non-struct or tree->child->sibling for structs
     */

    if (tree->child->type != T_TYPE_NAME)
    {
        log_tree(LOG_FATAL, tree->child, "Malformed AST (%s) seen in declaration!", token_get_name(tree->child->type));
    }

    if (scope->parentFunction == NULL)
    {
        walk_type_name(tree->child, scope, &declaredType, NULL);
    }
    else
    {
        walk_type_name(tree->child, scope, &declaredType, scope->parentFunction->implementedFor);
    }

    struct VariableEntry *declaredVariable = NULL;

    if (isArgument)
    {
        declaredVariable = scope_create_argument(scope, tree->child->sibling,
                                                 &declaredType,
                                                 A_PUBLIC);
    }
    else
    {
        // automatically set as a global if there is no parent scope (declaring at the outermost scope)
        declaredVariable = scope_create_variable(scope,
                                                 tree->child->sibling,
                                                 &declaredType,
                                                 (scope->parentScope == NULL),
                                                 A_PUBLIC);
    }

    return declaredVariable;
}

struct VariableEntry *walk_field_declaration(struct Ast *tree,
                                             struct Scope *scope,
                                             const size_t *tacIndex,
                                             const size_t *tempNum,
                                             enum ACCESS accessibility,
                                             struct TypeEntry *fieldOf)
{
    log_tree(LOG_DEBUG, tree, "walk_field_declaration");

    if (tree->type != T_VARIABLE_DECLARATION)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_field_declaration!", token_get_name(tree->type));
    }

    struct Type declaredType = {0};

    /* 'struct' trees' children are the struct name
     * other variables' children are the pointer or variable name
     * so we need to start at tree->child for non-struct or tree->child->sibling for structs
     */

    if (tree->child->type != T_TYPE_NAME)
    {
        log_tree(LOG_FATAL, tree->child, "Malformed AST (%s) seen in declaration!", token_get_name(tree->child->type));
    }

    walk_type_name(tree->child, scope, &declaredType, fieldOf);

    struct VariableEntry *declaredVariable = NULL;

    // automatically set as a global if there is no parent scope (declaring at the outermost scope)
    declaredVariable = scope_create_variable(scope,
                                             tree->child->sibling,
                                             &declaredType,
                                             (scope->parentScope == NULL),
                                             accessibility);

    return declaredVariable;
}

void walk_argument_declaration(struct Ast *tree,
                               size_t *tacIndex,
                               size_t *tempNum,
                               struct FunctionEntry *fun)
{
    log_tree(LOG_DEBUG, tree, "WalkArgumentDeclaration");

    struct VariableEntry *declaredArgument = walk_variable_declaration(tree, fun->mainScope, tacIndex, tempNum, true);

    deque_push_back(fun->arguments, declaredArgument);
}

void verify_function_signatures(struct Ast *tree, struct FunctionEntry *existingFunc, struct FunctionEntry *parsedFunc)
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
            struct VariableEntry *existingArg = deque_at(existingFunc->arguments, argIndex);
            struct VariableEntry *parsedArg = deque_at(parsedFunc->arguments, argIndex);
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
        log(LOG_DEBUG, "Mismatch in number of arguments between parsed function %s and existing function %s", parsedFunc->name, existingFunc->name);
        mismatch = 1;
    }

    if (mismatch)
    {
        printf("\nConflicting declarations of function:\n");

        // TODO: print correctly with deque
        char *existingReturnType = type_get_name(&existingFunc->returnType);
        printf("\t%s %s(", existingReturnType, existingFunc->name);
        free(existingReturnType);
        for (size_t argIndex = 0; argIndex < existingFunc->arguments->size; argIndex++)
        {
            struct VariableEntry *existingArg = deque_at(existingFunc->arguments, argIndex);

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
        for (size_t argIndex = 0; argIndex < existingFunc->arguments->size; argIndex++)
        {
            struct VariableEntry *parsedArg = deque_at(parsedFunc->arguments, argIndex);

            char *argType = type_get_name(&parsedArg->type);
            printf("%s %s", argType, parsedArg->name);
            free(argType);

            // TODO: iterator_has_next();
            if (argIndex < existingFunc->arguments->size - 1)
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

void insert_self_method_argument(struct FunctionEntry *function, struct VariableEntry *selfArgument)
{
    if (function->arguments->size > 0)
    {
        struct VariableEntry *firstArgument = deque_at(function->arguments, 0);
        if (strcmp(firstArgument->name, OUT_OBJECT_POINTER_NAME) == 0)
        {
            struct VariableEntry *outObjPtr = deque_pop_front(function->arguments);
            deque_push_front(function->arguments, selfArgument);
            deque_push_front(function->arguments, outObjPtr);
        }
        else
        {
            deque_push_front(function->arguments, selfArgument);
        }
    }
    else
    {
        deque_push_front(function->arguments, selfArgument);
    }
}

List *walk_generic_parameter_names(struct Ast *tree)
{
    log_tree(LOG_DEBUG, tree, "walk_generic_parameter_names");

    if (tree->type != T_GENERIC_PARAMETER_NAMES)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_generic_parameter_names!", token_get_name(tree->type));
    }

    List *parameters = list_new(NULL, (ssize_t(*)(void *, void *))strcmp);

    struct Ast *genericRunner = tree->child;
    while (genericRunner != NULL)
    {
        if (genericRunner->type != T_IDENTIFIER)
        {
            log_tree(LOG_FATAL, genericRunner, "Malformed AST seen in walk_generic_parameter_names!");
        }

        if (list_find(parameters, genericRunner->value) != NULL)
        {
            log_tree(LOG_FATAL, genericRunner, "Redifinition of generic parameter %s!", genericRunner->value);
        }

        list_append(parameters, genericRunner->value);

        genericRunner = genericRunner->sibling;
    }

    return parameters;
}

struct FunctionEntry *walk_function_declaration(struct Ast *tree,
                                                struct Scope *scope,
                                                struct TypeEntry *implementedFor,
                                                enum ACCESS accessibility,
                                                bool forTrait)
{
    log_tree(LOG_DEBUG, tree, "walk_function_declaration");

    if (tree->type != T_FUN)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_function_declaration!", token_get_name(tree->type));
    }

    // skip past the argumnent declarations to the return type declaration
    struct Ast *returnTypeTree = tree->child;

    // functions return nothing in the default case
    struct Ast *functionNameTree = NULL;

    struct FunctionEntry *parsedFunc = NULL;
    // if the function returns something, its return type will be the first child of the 'fun' token
    if (returnTypeTree->type == T_TYPE_NAME)
    {
        functionNameTree = returnTypeTree->sibling;
        parsedFunc = function_entry_new(scope, functionNameTree, implementedFor);

        walk_type_name(returnTypeTree, parsedFunc->mainScope, &parsedFunc->returnType, implementedFor);
    }
    else
    {
        // there actually is no return type tree, we just go directly to argument declarations
        functionNameTree = returnTypeTree;
        parsedFunc = function_entry_new(scope, functionNameTree, implementedFor);
    }

    // child is the lparen, function name is the child of the lparen
    struct ScopeMember *lookedUpFunction = scope_lookup(scope, functionNameTree->value, E_FUNCTION);
    struct FunctionEntry *existingFunc = NULL;
    struct FunctionEntry *returnedFunc = NULL;

    if (lookedUpFunction != NULL)
    {
        existingFunc = lookedUpFunction->entry;
        returnedFunc = existingFunc;
    }
    else
    {
        if (!forTrait)
        {
            scope_insert(scope, functionNameTree->value, parsedFunc, E_FUNCTION, accessibility);
        }
        returnedFunc = parsedFunc;
    }

    if (type_is_object(&parsedFunc->returnType))
    {
        struct Type outPointerType = type_duplicate_non_pointer(&parsedFunc->returnType);
        outPointerType.pointerLevel++;
        struct Ast outPointerTree = *tree;
        outPointerTree.type = T_IDENTIFIER;
        outPointerTree.value = OUT_OBJECT_POINTER_NAME;
        outPointerTree.child = NULL;
        outPointerTree.sibling = NULL;
        struct VariableEntry *outPointerArgument = scope_create_argument(parsedFunc->mainScope, &outPointerTree, &outPointerType, A_PUBLIC);
        deque_push_front(parsedFunc->arguments, outPointerArgument);
    }

    struct Ast *argumentRunner = functionNameTree->sibling;
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
            walk_argument_declaration(argumentRunner, &tacIndex, &tempNum, parsedFunc);
        }
        break;

        case T_SELF:
        {
            if ((implementedFor == NULL) && !forTrait)
            {
                log_tree(LOG_FATAL, argumentRunner, "Malformed AST within function declaration - saw self when (implementedFor == NULL) && (forTrait == false)");
            }
            struct Type selfType;
            type_init(&selfType);
            type_set_basic_type(&selfType, VT_SELF, NULL, 1);
            struct VariableEntry *selfArgument = scope_create_argument(parsedFunc->mainScope, argumentRunner, &selfType, A_PUBLIC);

            insert_self_method_argument(parsedFunc, selfArgument);
        }
        break;

        default:
            InternalError("Malformed AST within function - expected function name and main scope only!\nMalformed node was of type %s with value [%s]", token_get_name(argumentRunner->type), argumentRunner->value);
        }
        argumentRunner = argumentRunner->sibling;
    }

    if (existingFunc != NULL)
    {
        if (function_entry_compare(existingFunc, parsedFunc))
        {
            char *existingSignature = sprint_function_signature(existingFunc);
            char *parsedSignature = sprint_function_signature(parsedFunc);
            log_tree(LOG_FATAL, tree, "Conflicting declarations of function %s\nExisting: %s\n  Parsed: %s!", parsedFunc->name, existingSignature, parsedSignature);
        }
    }

    // free the basic block we used to Walk declarations of arguments
    basic_block_free(block);

    struct Ast *definition = argumentRunner;
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

void walk_function_definition(struct Ast *tree,
                              struct FunctionEntry *fun)
{
    log_tree(LOG_DEBUG, tree, "walk_function_definition");

    if ((tree->type != T_COMPOUND_STATEMENT) && (tree->type != T_ASM))
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_function_definition!", token_get_name(tree->type));
    }

    size_t tacIndex = 0;
    size_t tempNum = 0;
    ssize_t labelNum = FUNCTION_EXIT_BLOCK_LABEL + 1;
    struct BasicBlock *exitBlock = basic_block_new(FUNCTION_EXIT_BLOCK_LABEL);

    struct BasicBlock *entryBlock = basic_block_new(labelNum);
    labelNum++;

    scope_add_basic_block(fun->mainScope, entryBlock);
    if (tree->type == T_COMPOUND_STATEMENT)
    {
        // TODO: fix controlConvergesTo scheme
        // currently, control always ends jumping to a label for an empty block directly above the function_done label - this sucks.
        walk_scope(tree, entryBlock, fun->mainScope, &tacIndex, &tempNum, &labelNum, exitBlock->labelNum);
    }
    else
    {
        fun->isAsmFun = 1;
        walk_asm_block(tree, entryBlock, fun->mainScope, &tacIndex, &tempNum);
        struct TACLine *jumpToExit = new_tac_line(TT_JMP, tree);
        jumpToExit->operands.jump.label = FUNCTION_EXIT_BLOCK_LABEL;
        basic_block_append(entryBlock, jumpToExit, &tacIndex);
    }
    scope_add_basic_block(fun->mainScope, exitBlock);
}

struct FunctionEntry *walk_implemented_function(struct Ast *tree,
                                                struct TypeEntry *implementedFor,
                                                enum ACCESS accessibility)
{
    log(LOG_DEBUG, "walk_implemented_function", tree->sourceFile, tree->sourceLine, tree->sourceCol);

    if (tree->type != T_FUN)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_implemented_function!", token_get_name(tree->type));
    }

    struct FunctionEntry *walkedMethod = walk_function_declaration(tree, implementedFor->implemented, implementedFor, accessibility, false);
    type_entry_add_implemented(implementedFor, walkedMethod, accessibility);

    if (walkedMethod->arguments->size > 0)
    {
        struct VariableEntry *potentialSelfArg = deque_at(walkedMethod->arguments, 0);

        // if the first arg to the function is the address of a struct which we are returning
        // try and see if the second argument is self (if it exists)
        if (!strcmp(potentialSelfArg->name, OUT_OBJECT_POINTER_NAME) && (walkedMethod->arguments->size > 1))
        {
            potentialSelfArg = deque_at(walkedMethod->arguments, 1);
        }

        if ((potentialSelfArg->type.basicType == VT_SELF) ||
            ((potentialSelfArg->type.basicType == VT_STRUCT) && (strcmp(potentialSelfArg->type.nonArray.complexType.name, implementedFor->baseName) == 0)))
        {
            if (strcmp(potentialSelfArg->name, "self") == 0)
            {
                walkedMethod->isMethod = true;
            }
        }
    }

    return walkedMethod;
}

void walk_implementation(struct Ast *tree, struct TypeEntry *implementedFor)
{
    switch (tree->type)
    {
    case T_FUN:
        walk_implemented_function(tree, implementedFor, A_PRIVATE);
        break;

    case T_PUBLIC:
        walk_implemented_function(tree->child, implementedFor, A_PUBLIC);
        break;

    default:
        log_tree(LOG_FATAL, tree, "Malformed AST seen %s (%s) in walk_implementation!", token_get_name(tree->type), tree->value);
    }
}

void walk_basic_impl(struct Ast *tree, struct Scope *scope)
{
    log(LOG_DEBUG, "walk_basic_impl", tree->sourceFile, tree->sourceLine, tree->sourceCol);

    if (tree->type != T_IMPL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_basic_impl!", token_get_name(tree->type));
    }

    struct Ast *implementedTypeTree = tree->child;
    if (implementedTypeTree->type != T_TYPE_NAME)
    {
        log_tree(LOG_FATAL, implementedTypeTree, "Malformed AST seen in walk_basic_impl!");
    }

    struct Type implementedType = {0};
    walk_type_name(implementedTypeTree, scope, &implementedType, NULL);

    if (((implementedType.basicType != VT_STRUCT) && (implementedType.basicType != VT_ENUM)) || (implementedType.pointerLevel != 0))
    {
        log_tree(LOG_FATAL, implementedTypeTree, "Implementation block for type %s not supported yet!", type_get_name(&implementedType));
    }

    struct TypeEntry *implementedFor = scope_lookup_type(scope, &implementedType);

    struct Ast *implementationRunner = implementedTypeTree->sibling;
    while (implementationRunner != NULL)
    {
        walk_implementation(implementationRunner, implementedFor);
        implementationRunner = implementationRunner->sibling;
    }

    if (implementedFor->genericType != G_BASE)
    {
        log(LOG_DEBUG, "Resolving capital 'Self' for non-generic-base %s at end of implementation block", implementedFor->baseName);
        type_entry_resolve_capital_self(implementedFor);
    }
}

void walk_trait_impl(struct Ast *tree, struct Scope *scope)
{
    log(LOG_DEBUG, "walk_trait_impl", tree->sourceFile, tree->sourceLine, tree->sourceCol);

    if (tree->type != T_IMPL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_trait_impl!", token_get_name(tree->type));
    }

    struct Ast *traitForTree = tree->child;
    if (traitForTree->type != T_FOR)
    {
        log_tree(LOG_FATAL, traitForTree, "Malformed AST seen in walk_trait_impl!");
    }

    struct Ast *traitNameTree = traitForTree->child;
    if (traitNameTree->type != T_IDENTIFIER)
    {
        log_tree(LOG_FATAL, traitNameTree, "Malformed AST seen in walk_trait_impl!");
    }
    struct TraitEntry *implementedTrait = scope_lookup_trait(scope, traitNameTree);

    struct Ast *implementedTypeTree = traitForTree->child->sibling;
    if (implementedTypeTree->type != T_TYPE_NAME)
    {
        log_tree(LOG_FATAL, implementedTypeTree, "Malformed AST seen in walk_trait_impl!");
    }

    struct Type implementedType = {0};
    walk_type_name(implementedTypeTree, scope, &implementedType, NULL);

    struct TypeEntry *implementedFor = scope_lookup_type(scope, &implementedType);

    Set *implementedPrivate = set_new(NULL, function_entry_compare);
    Set *implementedPublic = set_new(NULL, function_entry_compare);

    struct Ast *traitBodyRunner = traitForTree->sibling;
    while (traitBodyRunner != NULL)
    {
        switch (traitBodyRunner->type)
        {
        case T_FUN:
            set_insert(implementedPrivate, walk_implemented_function(traitBodyRunner, implementedFor, A_PUBLIC));
            break;

        case T_PUBLIC:
            set_insert(implementedPublic, walk_implemented_function(traitBodyRunner->child, implementedFor, A_PUBLIC));
            break;

        default:
            log_tree(LOG_FATAL, traitBodyRunner, "Malformed AST seen in walk_trait_impl!");
        }
        traitBodyRunner = traitBodyRunner->sibling;
    }

    // important to do this before resolution of capital 'Self' this relies on function_entry_compare and any VT_SELF still being pre-resolution
    type_entry_verify_trait(tree, implementedFor, implementedTrait, implementedPrivate, implementedPublic);

    if (implementedFor->genericType != G_BASE)
    {
        log(LOG_DEBUG, "Resolving capital 'Self' for %s at end of trait implementation block", implementedFor->baseName);
        type_entry_resolve_capital_self(implementedFor);
    }
}

void walk_implementation_block(struct Ast *tree, struct Scope *scope)
{
    log(LOG_DEBUG, "walk_implementation_block", tree->sourceFile, tree->sourceLine, tree->sourceCol);

    if (tree->type != T_IMPL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_implementation_block!", token_get_name(tree->type));
    }

    switch (tree->child->type)
    {
    case T_TYPE_NAME:
        walk_basic_impl(tree, scope);
        break;

    case T_FOR:
        walk_trait_impl(tree, scope);
        break;

    default:
        log_tree(LOG_FATAL, tree->child, "Malformed AST seen in walk_implementation_block!");
    }
}

struct StructDesc *walk_struct_declaration(struct Ast *tree,
                                           struct Scope *scope,
                                           List *genericParams)
{
    log_tree(LOG_DEBUG, tree, "walk_struct_declaration");

    if (tree->type != T_STRUCT)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to WalkStructDefinition!", token_get_name(tree->type));
    }
    size_t dummyNum = 0;

    struct TypeEntry *declaredType = NULL;
    struct StructDesc *declaredStruct = NULL;
    if (tree->child->type == T_IDENTIFIER)
    {
        if (genericParams != NULL)
        {
            declaredType = scope_create_generic_base_struct(scope, tree->child->value, genericParams);
        }
        else
        {
            declaredType = scope_create_struct(scope, tree->child->value);
        }
    }
    else
    {
        log_tree(LOG_FATAL, tree->child, "Malformed AST (%s) seen in walk_struct_declaration!", token_get_name(tree->child->type));
    }

    declaredStruct = declaredType->data.asStruct;

    struct Ast *structBody = tree->child->sibling;

    if (structBody->type != T_STRUCT_BODY)
    {
        log_tree(LOG_FATAL, tree, "Malformed AST seen in WalkStructDefinition!");
    }

    struct Ast *structBodyRunner = structBody->child;
    while (structBodyRunner != NULL)
    {
        switch (structBodyRunner->type)
        {
        case T_VARIABLE_DECLARATION:
        {
            struct VariableEntry *declaredField = walk_field_declaration(structBodyRunner, declaredStruct->members, &dummyNum, &dummyNum, A_PRIVATE, declaredType);
            struct_add_field(declaredStruct, declaredField);
        }
        break;

        case T_PUBLIC:
        {
            struct VariableEntry *declaredField = walk_field_declaration(structBodyRunner->child, declaredStruct->members, &dummyNum, &dummyNum, A_PUBLIC, declaredType);
            struct_add_field(declaredStruct, declaredField);
        }
        break;

        default:
            log_tree(LOG_FATAL, structBodyRunner, "Wrong AST (%s) seen in body of struct definition!", token_get_name(structBodyRunner->type));
        }

        structBodyRunner = structBodyRunner->sibling;
    }

    if (declaredType->genericType != G_BASE)
    {
        log(LOG_DEBUG, "Resolving capital 'Self' and assigning offsets to fields for non-generic-base struct %s ", declaredStruct->name);
        type_entry_resolve_capital_self(declaredType);
        struct_assign_offsets_to_fields(declaredStruct);
    }

    return declaredStruct;
}

void compare_generic_params(struct Ast *genericParamsTree, List *actualParams, List *expectedParams, char *genericType, char *genericName)
{
    Iterator *actualIter = list_begin(actualParams);
    Iterator *expectedIter = list_begin(expectedParams);
    bool mismatch = false;
    while (iterator_gettable(actualIter) && iterator_gettable(expectedIter))
    {
        char *actualParam = iterator_get(actualIter);
        char *expectedParam = iterator_get(expectedIter);
        if (strcmp(actualParam, expectedParam) != 0)
        {
            mismatch = true;
            break;
        }

        iterator_next(actualIter);
        iterator_next(expectedIter);
    }

    if (iterator_gettable(actualIter) || iterator_gettable(expectedIter))
    {
        mismatch = true;
    }

    iterator_free(actualIter);
    iterator_free(expectedIter);

    if (mismatch)
    {
        char *actualStr = sprint_generic_param_names(actualParams);
        char *expectedStr = sprint_generic_param_names(expectedParams);
        log_tree(LOG_FATAL, genericParamsTree, "Mismatch between generic parameters for %s %s!\nExpected: %s<%s>\n  Actual: %s<%s>", genericType, genericName, genericName, expectedStr, genericName, actualStr);
    }
}

void walk_generic(struct Ast *tree,
                  struct Scope *scope)
{
    log_tree(LOG_DEBUG, tree, "walk_generic");

    if (tree->type != T_GENERIC)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_generic!", token_get_name(tree->type));
    }

    struct Ast *genericThing = tree->child->sibling;
    struct Ast *genericParamsTree = tree->child;

    List *genericParams = walk_generic_parameter_names(genericParamsTree);

    switch (genericThing->type)
    {
    case T_STRUCT:
    {
        walk_struct_declaration(genericThing, scope, genericParams);
    }
    break;

    case T_ENUM:
    {
        walk_enum_declaration(genericThing, NULL, scope, genericParams);
    }
    break;

    case T_IMPL:
    {
        struct Ast *implementedTypeTree = genericThing->child;
        if (implementedTypeTree->type != T_TYPE_NAME)
        {
            log_tree(LOG_FATAL, implementedTypeTree, "Malformed AST seen in WalkImplementation!");
        }

        struct Type implementedType = {0};
        walk_type_name(implementedTypeTree, scope, &implementedType, NULL);

        if (((implementedType.basicType != VT_STRUCT) && (implementedType.basicType != VT_ENUM)) || (implementedType.pointerLevel != 0))
        {
            log_tree(LOG_FATAL, implementedTypeTree, "Implementation block for type %s not supported yet!", type_get_name(&implementedType));
        }

        struct TypeEntry *implementedTypeEntry = scope_lookup_type(scope, &implementedType);
        compare_generic_params(genericParamsTree, genericParams, implementedTypeEntry->generic.instance.parameters, "struct", implementedTypeEntry->baseName);

        struct Ast *implementationRunner = genericThing->child->sibling;
        while (implementationRunner != NULL)
        {
            walk_implementation(implementationRunner, implementedTypeEntry);
            implementationRunner = implementationRunner->sibling;
        }
        list_free(genericParams);
    }
    break;

    default:
        log_tree(LOG_FATAL, genericThing, "Malformed AST (%s) seen as thing being genericized under T_GENERIC!", token_get_name(genericThing->type));
    }
}

void walk_trait_declaration(struct Ast *tree, struct Scope *scope)
{
    log_tree(LOG_DEBUG, tree, "walk_trait_declaration");

    if (tree->type != T_TRAIT)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_trait_declaration!", token_get_name(tree->type));
    }

    struct Ast *traitName = tree->child;
    struct Ast *traitBodyRunner = traitName->sibling;

    struct TraitEntry *declaredTrait = scope_create_trait(scope, traitName->value);

    while (traitBodyRunner != NULL)
    {
        struct FunctionEntry *implemented = NULL;

        bool isPublic = false;
        switch (traitBodyRunner->type)
        {
        case T_FUN:
            implemented = walk_function_declaration(traitBodyRunner, NULL, NULL, A_PRIVATE, true);
            break;

        case T_PUBLIC:
            isPublic = true;
            if (traitBodyRunner->child->type != T_FUN)
            {
                log_tree(LOG_FATAL, traitBodyRunner, "Malformed AST (%s) seen in trait body!", token_get_name(traitBodyRunner->child->type));
            }
            implemented = walk_function_declaration(traitBodyRunner->child, NULL, NULL, A_PUBLIC, true);
            break;

        default:
            log_tree(LOG_FATAL, traitBodyRunner, "Malformed AST (%s) seen in trait body!", token_get_name(traitBodyRunner->type));
        }

        if (implemented->isDefined)
        {
            log_tree(LOG_FATAL, traitBodyRunner, "Trait declaration of %s must be a prototype only!", implemented->name);
        }

        if (isPublic)
        {
            set_insert(declaredTrait->public, implemented);
        }
        else
        {
            set_insert(declaredTrait->private, implemented);
        }

        traitBodyRunner = traitBodyRunner->sibling;
    }
}

void walk_enum_declaration(struct Ast *tree,
                           struct BasicBlock *block,
                           struct Scope *scope,
                           List *genericParams)
{
    log_tree(LOG_DEBUG, tree, "walk_enum_declaration");

    if (tree->type != T_ENUM)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_enum_declaration!", token_get_name(tree->type));
    }

    struct Ast *enumName = tree->child;

    struct TypeEntry *declaredType = NULL;
    struct EnumDesc *declaredEnum = NULL;

    if (genericParams != NULL)
    {
        declaredType = scope_create_generic_base_enum(scope, enumName->value, genericParams);
    }
    else
    {
        declaredType = scope_create_enum(scope, enumName->value);
    }
    declaredEnum = declaredType->data.asEnum;

    for (struct Ast *enumRunner = enumName->sibling; enumRunner != NULL; enumRunner = enumRunner->sibling)
    {
        if (enumRunner->type != T_IDENTIFIER)
        {
            InternalError("Malformed AST (%s) seen while Walking enum element delcarations", token_get_name(enumRunner->type));
        }

        struct Type memberType;
        type_init(&memberType);

        if (enumRunner->child != NULL)
        {
            walk_type_name(enumRunner->child, scope, &memberType, declaredType);
        }

        log(LOG_DEBUG, "Adding enum member %s to enum %s", enumRunner->value, declaredEnum->name);
        enum_add_member(declaredEnum, enumRunner, &memberType);
    }

    if (declaredType->genericType != G_BASE)
    {
        log(LOG_DEBUG, "Resolving capital 'Self' and calculating union size to fields for non-generic-base enum %s ", declaredEnum->name);
        type_entry_resolve_capital_self(declaredType);
        enum_desc_calculate_union_size(declaredEnum);
    }
}

void walk_return(struct Ast *tree,
                 struct Scope *scope,
                 struct BasicBlock *block,
                 size_t *tacIndex,
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
    struct TacReturn *returnOperands = &returnLine->operands.return_;

    if (tree->child != NULL)
    {
        walk_sub_expression(tree->child, block, scope, tacIndex, tempNum, &returnOperands->returnValue);

        if (type_compare_allow_implicit_widening(tac_operand_get_type(&returnLine->operands.return_.returnValue), &scope->parentFunction->returnType))
        {
            char *expectedReturnType = type_get_name(&scope->parentFunction->returnType);
            char *actualReturnType = type_get_name(tac_operand_get_type(&returnLine->operands.return_.returnValue));
            log_tree(LOG_FATAL, tree->child, "Returned type %s does not match expected return type of %s", actualReturnType, expectedReturnType);
        }

        if (type_is_object(&scope->parentFunction->returnType))
        {
            struct TACOperand *copiedFrom = &returnOperands->returnValue;
            struct TACOperand addressCopiedTo = {0};
            struct VariableEntry *outStructPointer = scope_lookup_var_by_string(scope, OUT_OBJECT_POINTER_NAME);
            tac_operand_populate_from_variable(&addressCopiedTo, outStructPointer);

            struct TACLine *structReturnStore = new_tac_line(TT_STORE, tree);
            struct TacStore *storeOperands = &structReturnStore->operands.store;
            storeOperands->source = *copiedFrom;

            tac_operand_populate_from_variable(&storeOperands->address, outStructPointer);

            basic_block_append(block, structReturnStore, tacIndex);

            memset(&returnOperands->returnValue, 0, sizeof(struct TACOperand));
        }
    }

    basic_block_append(block, returnLine, tacIndex);

    if (tree->sibling != NULL)
    {
        log_tree(LOG_FATAL, tree->sibling, "Code after return statement is unreachable!");
    }
}

void walk_statement(struct Ast *tree,
                    struct BasicBlock **blockP,
                    struct Scope *scope,
                    size_t *tacIndex,
                    size_t *tempNum,
                    ssize_t *labelNum,
                    ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "WalkStatement");

    switch (tree->type)
    {
    case T_VARIABLE_DECLARATION:
        walk_variable_declaration(tree, scope, tacIndex, tempNum, false);
        break;

    case T_EXTERN:
        log_tree(LOG_FATAL, tree, "'extern' is only allowed at the global scope.");
        break;

    case T_ASSIGN:
        walk_assignment(tree, *blockP, scope, tacIndex, tempNum);
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
        walk_arithmetic_assignment(tree, *blockP, scope, tacIndex, tempNum);
        break;

    case T_WHILE:
    {
        struct BasicBlock *afterWhileBlock = basic_block_new((*labelNum)++);
        walk_while_loop(tree, *blockP, scope, tacIndex, tempNum, labelNum, afterWhileBlock->labelNum);
        *blockP = afterWhileBlock;
        scope_add_basic_block(scope, afterWhileBlock);
    }
    break;

    case T_IF:
    {
        struct BasicBlock *afterIfBlock = basic_block_new((*labelNum)++);
        walk_if_statement(tree, *blockP, scope, tacIndex, tempNum, labelNum, afterIfBlock->labelNum);
        *blockP = afterIfBlock;
        scope_add_basic_block(scope, afterIfBlock);
    }
    break;

    case T_FOR:
    {
        struct BasicBlock *afterForBlock = basic_block_new((*labelNum)++);
        walk_for_loop(tree, *blockP, scope, tacIndex, tempNum, labelNum, afterForBlock->labelNum);
        *blockP = afterForBlock;
        scope_add_basic_block(scope, afterForBlock);
    }
    break;

    case T_MATCH:
    {
        struct BasicBlock *afterMatchBlock = basic_block_new((*labelNum)++);
        walk_match_statement(tree, *blockP, scope, tacIndex, tempNum, labelNum, afterMatchBlock->labelNum);
        *blockP = afterMatchBlock;
        scope_add_basic_block(scope, afterMatchBlock);
    }
    break;

    case T_FUNCTION_CALL:
        walk_function_call(tree, *blockP, scope, tacIndex, tempNum, NULL);
        break;

    case T_METHOD_CALL:
        walk_method_call(tree, *blockP, scope, tacIndex, tempNum, NULL);
        break;

    // subscope
    case T_COMPOUND_STATEMENT:
    {
        // TODO: is there a bug here for simple scopes within code (not attached to if/while/etc... statements? TAC dump for the scopes test seems to indicate so?)
        struct Scope *subScope = scope_create_sub_scope(scope);
        struct BasicBlock *afterSubScopeBlock = basic_block_new((*labelNum)++);
        walk_scope(tree, *blockP, subScope, tacIndex, tempNum, labelNum, afterSubScopeBlock->labelNum);
        *blockP = afterSubScopeBlock;
        scope_add_basic_block(scope, afterSubScopeBlock);
    }
    break;

    case T_RETURN:
        walk_return(tree, scope, *blockP, tacIndex, tempNum);
        break;

    case T_ASM:
        walk_asm_block(tree, *blockP, scope, tacIndex, tempNum);
        break;

    default:
        log_tree(LOG_FATAL, tree, "Unexpected AST type (%s - %s) seen in WalkStatement!", token_get_name(tree->type), tree->value);
    }
}

void walk_scope(struct Ast *tree,
                struct BasicBlock *block,
                struct Scope *scope,
                size_t *tacIndex,
                size_t *tempNum,
                ssize_t *labelNum,
                ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "walk_scope");

    if (tree->type != T_COMPOUND_STATEMENT)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_scope!", token_get_name(tree->type));
    }

    struct Ast *scopeRunner = tree->child;
    while (scopeRunner != NULL)
    {
        walk_statement(scopeRunner, &block, scope, tacIndex, tempNum, labelNum, controlConvergesToLabel);
        scopeRunner = scopeRunner->sibling;
    }

    if (controlConvergesToLabel >= 0)
    {
        struct TACLine *controlConvergeJmp = new_tac_line(TT_JMP, tree);
        struct TacJump *jumpOperands = &controlConvergeJmp->operands.jump;
        jumpOperands->label = controlConvergesToLabel;
        basic_block_append(block, controlConvergeJmp, tacIndex);
    }
}

struct BasicBlock *walk_logical_operator(struct Ast *tree,
                                         struct BasicBlock *block,
                                         struct Scope *scope,
                                         size_t *tacIndex,
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
        block = walk_condition_check(tree->child, block, scope, tacIndex, tempNum, labelNum, falseJumpLabelNum);
        block = walk_condition_check(tree->child->sibling, block, scope, tacIndex, tempNum, labelNum, falseJumpLabelNum);
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
        block = walk_condition_check(tree->child, block, scope, tacIndex, tempNum, labelNum, checkSecondConditionBlock->labelNum);

        // if we pass the first condition (don't jump to checkSecondConditionBlock), short-circuit directly to the true block
        struct TACLine *firstConditionTrueJump = new_tac_line(TT_JMP, tree->child);
        firstConditionTrueJump->operands.jump.label = trueBlock->labelNum;
        basic_block_append(block, firstConditionTrueJump, tacIndex);

        // Walk the second condition to checkSecondConditionBlock
        block = walk_condition_check(tree->child->sibling, checkSecondConditionBlock, scope, tacIndex, tempNum, labelNum, falseJumpLabelNum);

        // jump from whatever block the second condition check ends up in (passing path) to our block
        // this ensures that regardless of which condition is true (first or second) execution always end up in the same block
        struct TACLine *secondConditionTrueJump = new_tac_line(TT_JMP, tree->child->sibling);
        secondConditionTrueJump->operands.jump.label = trueBlock->labelNum;
        basic_block_append(block, secondConditionTrueJump, tacIndex);

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

        block = walk_condition_check(tree->child, block, scope, tacIndex, tempNum, labelNum, inverseConditionBlock->labelNum);

        // subcondition is true (!subcondition is false), then control flow should end up at the original conditionFalseJump destination
        struct TACLine *conditionFalseJump = new_tac_line(TT_JMP, tree->child);
        conditionFalseJump->operands.jump.label = falseJumpLabelNum;
        basic_block_append(block, conditionFalseJump, tacIndex);

        // return the tricky block we created to be jumped to when our subcondition is false, or that the condition we are linearizing at this level is true
        block = inverseConditionBlock;
    }
    break;

    default:
        InternalError("Logical operator %s (%s) not supported yet",
                      token_get_name(tree->type),
                      tree->value);
    }

    return block;
}

struct BasicBlock *walk_condition_check(struct Ast *tree,
                                        struct BasicBlock *block,
                                        struct Scope *scope,
                                        size_t *tacIndex,
                                        size_t *tempNum,
                                        ssize_t *labelNum,
                                        ssize_t falseJumpLabelNum)
{
    log_tree(LOG_DEBUG, tree, "walk_condition_check");

    struct TACLine *condFalseJump = new_tac_line(TT_BEQ, tree);
    condFalseJump->operands.conditionalBranch.label = falseJumpLabelNum;

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
        block = walk_logical_operator(tree, block, scope, tacIndex, tempNum, labelNum, falseJumpLabelNum);
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
                walk_sub_expression(tree->child, block, scope, tacIndex, tempNum, &condFalseJump->operands.conditionalBranch.sourceA);
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
                walk_sub_expression(tree->child->sibling, block, scope, tacIndex, tempNum, &condFalseJump->operands.conditionalBranch.sourceB);
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
        walk_sub_expression(tree, block, scope, tacIndex, tempNum, &condFalseJump->operands.conditionalBranch.sourceA);

        condFalseJump->operands.conditionalBranch.sourceB.castAsType.basicType = VT_U8;
        condFalseJump->operands.conditionalBranch.sourceB.permutation = VP_LITERAL_VAL;
        condFalseJump->operands.conditionalBranch.sourceB.name.str = 0;
    }
    break;

    default:
    {
        InternalError("Comparison operator %s (%s) not supported yet",
                      token_get_name(tree->type),
                      tree->value);
    }
    break;
    }

    if (condFalseJump != NULL)
    {
        basic_block_append(block, condFalseJump, tacIndex);
    }
    return block;
}

void walk_while_loop(struct Ast *tree,
                     struct BasicBlock *block,
                     struct Scope *scope,
                     size_t *tacIndex,
                     size_t *tempNum,
                     ssize_t *labelNum,
                     ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "walk_while_loop");

    if (tree->type != T_WHILE)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_while_loop!", token_get_name(tree->type));
    }

    struct BasicBlock *beforeWhileBlock = block;

    struct TACLine *enterWhileJump = new_tac_line(TT_JMP, tree);
    enterWhileJump->operands.jump.label = *labelNum;
    basic_block_append(beforeWhileBlock, enterWhileJump, tacIndex);

    // create a subscope from which we will work
    struct Scope *whileScope = scope_create_sub_scope(scope);
    struct BasicBlock *whileBlock = basic_block_new((*labelNum)++);
    scope_add_basic_block(whileScope, whileBlock);

    struct TACLine *whileDo = new_tac_line(TT_DO, tree);
    basic_block_append(whileBlock, whileDo, tacIndex);

    whileBlock = walk_condition_check(tree->child, whileBlock, whileScope, tacIndex, tempNum, labelNum, controlConvergesToLabel);

    ssize_t endWhileLabel = (*labelNum)++;

    struct Ast *whileBody = tree->child->sibling;
    if (whileBody->type == T_COMPOUND_STATEMENT)
    {
        walk_scope(whileBody, whileBlock, whileScope, tacIndex, tempNum, labelNum, endWhileLabel);
    }
    else
    {
        walk_statement(whileBody, &whileBlock, whileScope, tacIndex, tempNum, labelNum, endWhileLabel);
    }

    struct TACLine *whileLoopJump = new_tac_line(TT_JMP, tree);
    whileLoopJump->operands.jump.label = enterWhileJump->operands.jump.label;

    block = basic_block_new(endWhileLabel);
    scope_add_basic_block(scope, block);

    struct TACLine *whileEndDo = new_tac_line(TT_ENDDO, tree);
    basic_block_append(block, whileLoopJump, tacIndex);
    basic_block_append(block, whileEndDo, tacIndex);
}

void walk_if_statement(struct Ast *tree,
                       struct BasicBlock *block,
                       struct Scope *scope,
                       size_t *tacIndex,
                       size_t *tempNum,
                       ssize_t *labelNum,
                       ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "walk_if_statement");

    if (tree->type != T_IF)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_if_statement!", token_get_name(tree->type));
    }

    size_t maxExitTACIndex = *tacIndex;

    struct Scope *ifScope = scope_create_sub_scope(scope);
    struct BasicBlock *ifBlock = basic_block_new((*labelNum)++);
    scope_add_basic_block(ifScope, ifBlock);

    struct TACLine *enterIfJump = new_tac_line(TT_JMP, tree);
    enterIfJump->operands.jump.label = ifBlock->labelNum;

    ssize_t falseJumpLabelNum = controlConvergesToLabel;
    struct Ast *elseTree = tree->child->sibling->sibling;
    if (elseTree != NULL)
    {
        falseJumpLabelNum = (*labelNum)++;
    }

    block = walk_condition_check(tree->child, block, scope, tacIndex, tempNum, labelNum, falseJumpLabelNum);

    size_t ifTACIndex = *tacIndex;

    basic_block_append(block, enterIfJump, tacIndex);

    struct Ast *ifBody = tree->child->sibling;
    if (ifBody->type == T_COMPOUND_STATEMENT)
    {
        walk_scope(ifBody, ifBlock, ifScope, &ifTACIndex, tempNum, labelNum, controlConvergesToLabel);
    }
    else
    {
        walk_statement(ifBody, &ifBlock, ifScope, &ifTACIndex, tempNum, labelNum, controlConvergesToLabel);
    }

    maxExitTACIndex = MAX(maxExitTACIndex, ifTACIndex);

    // if we have an else block
    if (elseTree != NULL)
    {
        struct Scope *elseScope = scope_create_sub_scope(scope);
        struct BasicBlock *elseBlock = basic_block_new(falseJumpLabelNum);

        size_t elseTACIndex = *tacIndex;

        scope_add_basic_block(elseScope, elseBlock);

        if (elseTree->type == T_COMPOUND_STATEMENT)
        {
            walk_scope(elseTree, elseBlock, elseScope, &elseTACIndex, tempNum, labelNum, controlConvergesToLabel);
        }
        else
        {
            walk_statement(elseTree, &elseBlock, elseScope, &elseTACIndex, tempNum, labelNum, controlConvergesToLabel);
        }

        maxExitTACIndex = MAX(maxExitTACIndex, elseTACIndex);
    }

    *tacIndex = MAX(*tacIndex, maxExitTACIndex);
}

void walk_for_loop(struct Ast *tree,
                   struct BasicBlock *block,
                   struct Scope *scope,
                   size_t *tacIndex,
                   size_t *tempNum,
                   ssize_t *labelNum,
                   ssize_t controlConvergesToLabel)
{
    log_tree(LOG_DEBUG, tree, "walk_for_loop");

    if (tree->type != T_FOR)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_for_loop!", token_get_name(tree->type));
    }

    struct Scope *forScope = scope_create_sub_scope(scope);
    struct BasicBlock *beforeForBlock = basic_block_new((*labelNum)++);
    scope_add_basic_block(forScope, beforeForBlock);

    struct TACLine *enterForScopeJump = new_tac_line(TT_JMP, tree);
    enterForScopeJump->operands.jump.label = beforeForBlock->labelNum;
    basic_block_append(block, enterForScopeJump, tempNum);

    struct Ast *forStartExpression = tree->child;
    struct Ast *forCondition = tree->child->sibling;

    //                       for   e1      e2       e3
    struct Ast *forAction = tree->child->sibling->sibling;
    // if the third expression has no sibling, it isn't actually a third expression but the body (and there is no third expression)
    if (forAction->sibling == NULL)
    {
        forAction = NULL;
    }
    walk_statement(forStartExpression, &beforeForBlock, forScope, tacIndex, tempNum, labelNum, controlConvergesToLabel);

    struct TACLine *enterForJump = new_tac_line(TT_JMP, tree);
    enterForJump->operands.jump.label = (*labelNum);
    basic_block_append(beforeForBlock, enterForJump, tacIndex);

    // create a subscope from which we will work
    struct BasicBlock *forBlock = basic_block_new((*labelNum)++);

    struct TACLine *whileDo = new_tac_line(TT_DO, tree);
    basic_block_append(forBlock, whileDo, tacIndex);
    scope_add_basic_block(forScope, forBlock);

    forBlock = walk_condition_check(forCondition, forBlock, forScope, tacIndex, tempNum, labelNum, controlConvergesToLabel);

    ssize_t endForLabel = (*labelNum)++;

    struct Ast *forBody = tree->child;
    while (forBody->sibling != NULL)
    {
        forBody = forBody->sibling;
    }
    if (forBody->type == T_COMPOUND_STATEMENT)
    {
        walk_scope(forBody, forBlock, forScope, tacIndex, tempNum, labelNum, endForLabel);
    }
    else
    {
        walk_statement(forBody, &forBlock, forScope, tacIndex, tempNum, labelNum, endForLabel);
    }

    struct BasicBlock *forActionBlock = basic_block_new(endForLabel);
    scope_add_basic_block(forScope, forActionBlock);

    if (forAction != NULL)
    {
        walk_statement(forAction, &forActionBlock, forScope, tacIndex, tempNum, labelNum, controlConvergesToLabel);
    }

    struct TACLine *forLoopJump = new_tac_line(TT_JMP, tree);
    forLoopJump->operands.jump.label = enterForJump->operands.jump.label;

    struct TACLine *forEndDo = new_tac_line(TT_ENDDO, tree);
    basic_block_append(forActionBlock, forLoopJump, tacIndex);
    basic_block_append(forActionBlock, forEndDo, tacIndex);
}

ssize_t walk_match_case_block(struct Ast *statement,
                              struct BasicBlock *caseBlock,
                              struct Scope *scope,
                              size_t *tacIndex,
                              size_t *tempNum,
                              ssize_t *labelNum,
                              ssize_t controlConvergesToLabel)
{
    ssize_t caseEntryLabel = caseBlock->labelNum;

    if (statement != NULL)
    {
        walk_statement(statement, &caseBlock, scope, tacIndex, tempNum, labelNum, controlConvergesToLabel);
    }

    // make sure every case ends up at the convergence block after the match
    struct TACLine *exitCaseJump = new_tac_line(TT_JMP, statement);
    exitCaseJump->operands.jump.label = controlConvergesToLabel;
    basic_block_append(caseBlock, exitCaseJump, tacIndex);

    return caseEntryLabel;
}

void check_match_cases(struct Ast *matchTree, struct Type *matchedType, struct EnumDesc *matchedEnum, Set *matchedValues)
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
        log_tree(LOG_FATAL, matchTree, "There is no conceivable way you wrote U64_MAX match cases for this match against a u64. Something is broken.");
        break;
    case VT_ENUM:
        stateSpaceSize = matchedEnum->members->size;
        break;
    case VT_ANY:
        break;
    case VT_NULL:
        InternalError("VT_NULL seen as type of matched expression");
    case VT_STRUCT:
        InternalError("VT_STRUCT seen as type of matched expression");
    case VT_ARRAY:
        InternalError("VT_STRUCT seen as type of matched expression");
    case VT_GENERIC_PARAM:
        InternalError("VT_GENERIC_PARAM seen as type of matched expression");
    case VT_SELF:
        InternalError("VT_SELF seen as type of matched expression");
    }

    size_t missingCases = matchedValues->size - stateSpaceSize;

    if (missingCases > 0)
    {
        char *pluralString = "";
        if (missingCases > 1)
        {
            pluralString = "s";
        }
        log_tree(LOG_FATAL, matchTree, "Missing %zu match case%s for type %s", stateSpaceSize - matchedValues->size, pluralString, type_get_name(matchedType));
    }
}

void walk_enum_match_arm(struct Ast *matchedValueTree,
                         struct BasicBlock *block,
                         struct Scope *scope,
                         size_t *tacIndex,
                         size_t *tempNum,
                         ssize_t *labelNum,
                         ssize_t controlConvergesToLabel,
                         struct Ast *actionTree,
                         bool *haveUnderscoreCase,
                         struct Ast **underscoreAction,
                         struct TACOperand *matchedAgainstEnum,
                         struct TACOperand *matchedAgainstNumerical,
                         struct EnumDesc *matchedEnum,
                         Set *matchedValues)
{
    // only allow underscore or identifier trees
    switch (matchedValueTree->type)
    {
    case T_UNDERSCORE:
        if (*haveUnderscoreCase)
        {
            log_tree(LOG_FATAL, matchedValueTree, "Duplicated underscore case");
        }
        *haveUnderscoreCase = true;
        *underscoreAction = actionTree;
        break;

    case T_IDENTIFIER:
    {
        struct EnumMember *matchedMember = enum_lookup_member(matchedEnum, matchedValueTree);

        if (set_find(matchedValues, &matchedMember->numerical) != NULL)
        {
            log_tree(LOG_FATAL, matchedValueTree, "Duplicated match case %s", matchedValueTree->value);
        }

        size_t *matchedValuePointer = malloc(sizeof(size_t));
        *matchedValuePointer = matchedMember->numerical;
        set_insert(matchedValues, matchedValuePointer);

        struct TACLine *matchJump = new_tac_line(TT_BEQ, matchedValueTree);

        matchJump->operands.conditionalBranch.sourceA.name.val = *matchedValuePointer;
        matchJump->operands.conditionalBranch.sourceA.castAsType = *tac_operand_get_type(matchedAgainstNumerical);
        matchJump->operands.conditionalBranch.sourceA.permutation = VP_LITERAL_VAL;

        matchJump->operands.conditionalBranch.sourceB = *matchedAgainstNumerical;
        matchJump->operands.conditionalBranch.sourceB.castAsType.basicType = VT_U64; // TODO: size_t definition?

        basic_block_append(block, matchJump, tacIndex);

        struct Scope *armScope = scope;

        struct BasicBlock *caseBlock = basic_block_new((*labelNum)++);

        // if we are mapping the type tagged as this enum/union member to an identifier
        if (matchedValueTree->child != NULL)
        {
            struct Ast *matchedDataName = matchedValueTree->child;

            // make sure we actually expect to map to some type for this member
            if (matchedMember->type.basicType == VT_NULL)
            {
                log_tree(LOG_FATAL, matchedDataName, "Attempt to map data in enum %s member %s to identifier %s, but member %s has no associated data", matchedEnum->name,
                         matchedMember->name,
                         matchedDataName->value,
                         matchedMember->name);
            }

            armScope = scope_create_sub_scope(scope);
            struct VariableEntry *dataVariable = scope_create_variable(armScope, matchedDataName, &matchedMember->type, false, A_PUBLIC);

            struct TACOperand *addrOfMatchedAgainst = get_addr_of_operand(matchedDataName, caseBlock, scope, tacIndex, tempNum, matchedAgainstEnum);
            struct TACLine *compAddrOfEnumData = new_tac_line(TT_ADD, matchedDataName);
            compAddrOfEnumData->operands.arithmetic.sourceA = *addrOfMatchedAgainst;

            // the actual data of the enum is at base + sizeof(size_t), so compute that address
            compAddrOfEnumData->operands.arithmetic.sourceB.name.val = sizeof(size_t);
            compAddrOfEnumData->operands.arithmetic.sourceB.permutation = VP_LITERAL_VAL;
            compAddrOfEnumData->operands.arithmetic.sourceB.castAsType.basicType = select_variable_type_for_number(sizeof(size_t));

            tac_operand_populate_as_temp(scope, &compAddrOfEnumData->operands.arithmetic.destination, tempNum, tac_operand_get_type(&compAddrOfEnumData->operands.arithmetic.sourceA));
            basic_block_append(caseBlock, compAddrOfEnumData, tacIndex);

            // then, do the actual load from the computed address to the temporary
            struct TACLine *dataExtractionLine = new_tac_line(TT_LOAD, matchedDataName);
            tac_operand_populate_from_variable(&dataExtractionLine->operands.load.destination, dataVariable);

            dataExtractionLine->operands.load.address = compAddrOfEnumData->operands.addrof.destination;
            dataExtractionLine->operands.load.address.castAsType = matchedMember->type;
            dataExtractionLine->operands.load.address.castAsType.pointerLevel++;

            basic_block_append(caseBlock, dataExtractionLine, tacIndex);
        }
        scope_add_basic_block(armScope, caseBlock);
        matchJump->operands.conditionalBranch.label = walk_match_case_block(actionTree, caseBlock, armScope, tacIndex, tempNum, labelNum, controlConvergesToLabel);
    }
    break;

    default:
        log_tree(LOG_FATAL, matchedValueTree, "Match against %s invalid for match against enum %s", matchedValueTree->value, matchedEnum->name);
        break;
    }
}

void walk_non_enum_match_arm(struct Ast *matchedValueTree,
                             struct BasicBlock *block,
                             struct Scope *scope,
                             size_t *tacIndex,
                             size_t *tempNum,
                             ssize_t *labelNum,
                             ssize_t controlConvergesToLabel,
                             struct Ast *actionTree,
                             bool *haveUnderscoreCase,
                             struct Ast **underscoreAction,
                             struct TACOperand *matchedAgainstEnum,
                             struct TACOperand *matchedAgainstNumerical,
                             struct Type *matchedType,
                             Set *matchedValues)
{
    // only allow underscore or constant trees
    switch (matchedValueTree->type)
    {
    case T_UNDERSCORE:
        if (*haveUnderscoreCase)
        {
            log_tree(LOG_FATAL, matchedValueTree, "Duplicated underscore case");
        }
        *haveUnderscoreCase = true;
        *underscoreAction = actionTree;
        break;

    case T_CONSTANT:
    case T_CHAR_LITERAL:
    {
        size_t matchedValue;
        if (matchedValueTree->type == T_CHAR_LITERAL)
        {
            matchedValue = (size_t)matchedValueTree->value[0];
        }

        else
        {
            if (strncmp(matchedValueTree->value, "0x", 2) == 0)
            {
                matchedValue = parse_hex_constant(matchedValueTree->value);
            }
            else
            {
                // TODO: abstract this
                matchedValue = atoi(matchedValueTree->value);
            }
        }

        if (set_find(matchedValues, &matchedValue) != NULL)
        {
            log_tree(LOG_FATAL, matchedValueTree, "Duplicated match case %s", matchedValueTree->value);
        }

        size_t *matchedValuePointer = malloc(sizeof(size_t));
        *matchedValuePointer = matchedValue;
        set_insert(matchedValues, matchedValuePointer);

        struct TACLine *matchJump = new_tac_line(TT_BEQ, matchedValueTree);

        // TODO: tac_operand_populate_as_literal
        matchJump->operands.conditionalBranch.sourceA.permutation = VP_LITERAL_VAL;
        matchJump->operands.conditionalBranch.sourceA.name.val = matchedValue;
        matchJump->operands.conditionalBranch.sourceA.castAsType = *tac_operand_get_type(matchedAgainstNumerical);

        matchJump->operands.conditionalBranch.sourceB = *matchedAgainstNumerical;

        basic_block_append(block, matchJump, tacIndex);

        struct BasicBlock *caseBlock = basic_block_new((*labelNum)++);
        scope_add_basic_block(scope, caseBlock);
        matchJump->operands.conditionalBranch.label = walk_match_case_block(actionTree, caseBlock, scope, tacIndex, tempNum, labelNum, controlConvergesToLabel);
    }
    break;

    case T_IDENTIFIER:
        log_tree(LOG_FATAL, matchedValueTree, "Match against identifier %s invalid for match against type %s", matchedValueTree->value, type_get_name(matchedType));
        break;

    default:
        log_tree(LOG_FATAL, matchedValueTree, "Malformed AST (%s) seen in cases of match statement!", token_get_name(matchedValueTree->type));
    }
}

void walk_match_statement(struct Ast *tree,
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
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_match_statement!", token_get_name(tree->type));
    }

    struct Ast *matchedExpression = tree->child;

    struct Ast *matchRunner = matchedExpression->sibling;

    Set *matchedValues = set_new(free, sizet_pointer_compare);

    struct TACOperand matchedAgainst = {0};
    walk_sub_expression(matchedExpression, block, scope, tacIndex, tempNum, &matchedAgainst);

    size_t maxExitTacIndex = *tacIndex;

    struct TACOperand matchedAgainstNumerical;
    // if matching against an enum, we need to do some manipulation to extract the actual numerical value associated with the enum
    if (type_is_enum_object(tac_operand_get_type(&matchedAgainst)))
    {
        struct TACOperand *addrOfMatchedAgainst = get_addr_of_operand(tree, block, scope, tacIndex, tempNum, &matchedAgainst);
        addrOfMatchedAgainst->castAsType.basicType = VT_U64; // TODO: size_t define
        addrOfMatchedAgainst->castAsType.pointerLevel = 1;

        struct TACLine *loadMatchedAgainst = new_tac_line(TT_LOAD, tree);

        loadMatchedAgainst->operands.load.address = *addrOfMatchedAgainst;

        struct Type matchedAgainstType = type_duplicate_non_pointer(tac_operand_get_type(addrOfMatchedAgainst));
        matchedAgainstType.pointerLevel--;
        tac_operand_populate_as_temp(scope, &loadMatchedAgainst->operands.load.destination, tempNum, &matchedAgainstType);
        matchedAgainstNumerical = loadMatchedAgainst->operands.load.destination;
        basic_block_append(block, loadMatchedAgainst, tacIndex);
    }
    else // not matching against an enum, so just cast to a size_t
    {
        matchedAgainstNumerical = matchedAgainst;
        matchedAgainstNumerical.castAsType.basicType = VT_U64; // TODO: size_t define
        matchedAgainstNumerical.castAsType.pointerLevel = 0;
    }

    struct Type *matchedType = tac_operand_get_type(&matchedAgainst);

    if (matchedType->pointerLevel > 0)
    {
        log_tree(LOG_FATAL, tree, "Match against pointer type (%s) forbidden!", type_get_name(matchedType));
    }

    if (type_is_struct_object(matchedType))
    {
        log_tree(LOG_FATAL, tree, "Match against struct type (%s) forbidden!", type_get_name(matchedType));
    }

    struct EnumDesc *matchedEnum = NULL;
    if (matchedType->basicType == VT_ENUM)
    {
        matchedEnum = scope_lookup_enum_by_type(scope, matchedType);
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
        case VT_GENERIC_PARAM:
        case VT_SELF:
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
    struct Ast *underscoreAction = NULL;

    while (matchRunner != NULL)
    {
        // match arms may resolve to empty statements, which will have no associated AST.
        // if there is an actual tree associated with this match arm, track it
        struct Ast *matchArmAction = NULL;
        if (matchRunner->child->child != NULL)
        {
            matchArmAction = matchRunner->child->child;
        }

        // TODO: only linearize each match arm action once instead of in each call to walk_*_match_arm
        struct Ast *matchedValueRunner = matchRunner->child->sibling;

        // for each case matched
        while (matchedValueRunner != NULL)
        {
            (*tacIndex) += 1;
            size_t armTacIndex = *tacIndex;
            // if we are matching against an enum
            if (matchedType->basicType == VT_ENUM)
            {
                walk_enum_match_arm(matchedValueRunner,
                                    block,
                                    scope,
                                    &armTacIndex,
                                    tempNum,
                                    labelNum,
                                    controlConvergesToLabel,
                                    matchArmAction,
                                    &haveUnderscoreCase,
                                    &underscoreAction,
                                    &matchedAgainst,
                                    &matchedAgainstNumerical,
                                    matchedEnum,
                                    matchedValues);
            }
            else // not matching against an enum
            {
                walk_non_enum_match_arm(matchedValueRunner,
                                        block,
                                        scope,
                                        &armTacIndex,
                                        tempNum,
                                        labelNum,
                                        controlConvergesToLabel,
                                        matchArmAction,
                                        &haveUnderscoreCase,
                                        &underscoreAction,
                                        &matchedAgainst,
                                        &matchedAgainstNumerical,
                                        matchedType,
                                        matchedValues);
            }

            maxExitTacIndex = MAX(maxExitTacIndex, armTacIndex);
            matchedValueRunner = matchedValueRunner->sibling;
        }
        matchRunner = matchRunner->sibling;
    }

    // if there is a catch-all underscore, fall through to its block at the very end of all our comparisons
    if (haveUnderscoreCase)
    {
        struct TACLine *underscoreJump = NULL;
        if (underscoreAction != NULL)
        {
            underscoreJump = new_tac_line(TT_JMP, underscoreAction);
            struct BasicBlock *caseBlock = basic_block_new((*labelNum)++);
            scope_add_basic_block(scope, caseBlock);
            size_t underscoreTacIndex = (*tacIndex + 1);
            underscoreJump->operands.jump.label = walk_match_case_block(underscoreAction, caseBlock, scope, &underscoreTacIndex, tempNum, labelNum, controlConvergesToLabel);
            maxExitTacIndex = MAX(maxExitTacIndex, underscoreTacIndex);
        }
        else
        {
            underscoreJump = new_tac_line(TT_JMP, tree);
            underscoreJump->operands.jump.label = controlConvergesToLabel;
        }
        basic_block_append(block, underscoreJump, tacIndex);
    }
    // no catch-all underscore, make sure that all possible cases are enumerated
    else
    {
        check_match_cases(tree, matchedType, matchedEnum, matchedValues);
    }

    struct TACLine *matchDoneJump = new_tac_line(TT_JMP, tree);
    matchDoneJump->operands.jump.label = controlConvergesToLabel;
    basic_block_append(block, matchDoneJump, tacIndex);

    // reconcile tac index, remember that we may have had more branching operations than tac lines in any given match arm so the actual tacIndex itself may be the max
    *tacIndex = MAX(*tacIndex, maxExitTacIndex);

    set_free(matchedValues);
}

void check_assignment_operand_types(struct Ast *tree,
                                    struct Type *sourceType,
                                    struct Type *destType)
{
    if (type_compare_allow_implicit_widening(sourceType, destType))
    {
        log_tree(LOG_FATAL, tree, "Assignment from type %s to type %s is not allowed!", type_get_name(sourceType), type_get_name(destType));
    }
}

void walk_assignment(struct Ast *tree,
                     struct BasicBlock *block,
                     struct Scope *scope,
                     size_t *tacIndex,
                     size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_assignment");

    if (tree->type != T_ASSIGN)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_assignment!", token_get_name(tree->type));
    }

    struct Ast *lhs = tree->child;
    struct Ast *rhs = tree->child->sibling;

    // don't increment the index until after we deal with nested expressions
    struct TACLine *assignment = new_tac_line(TT_ASSIGN, tree);

    struct TACOperand assignedValue = {0};

    // if we have anything but an initializer on the RHS, Walk it as a subexpression and save for later
    if (rhs->type != T_INITIALIZER)
    {
        walk_sub_expression(rhs, block, scope, tacIndex, tempNum, &assignedValue);
    }

    struct VariableEntry *assignedVariable = NULL;
    switch (lhs->type)
    {
    case T_VARIABLE_DECLARATION:
        assignedVariable = walk_variable_declaration(lhs, scope, tacIndex, tempNum, 0);
        tac_operand_populate_from_variable(&assignment->operands.assign.destination, assignedVariable);
        assignment->operands.assign.source = assignedValue;

        if (assignedVariable->type.basicType == VT_ARRAY)
        {
            char *arrayName = type_get_name(&assignedVariable->type);
            log_tree(LOG_FATAL, tree, "Assignment to local array variable %s with type %s is not allowed!", assignedVariable->name, arrayName);
        }
        break;

    case T_IDENTIFIER:
        assignedVariable = scope_lookup_var(scope, lhs);
        tac_operand_populate_from_variable(&assignment->operands.assign.destination, assignedVariable);
        assignment->operands.assign.source = assignedValue;
        break;

    // TODO: generate optimized addressing modes for arithmetic
    case T_DEREFERENCE:
    {
        struct Ast *writtenPointer = lhs->child;
        assignment->operation = TT_STORE;
        switch (writtenPointer->type)
        {
        case T_ADD:
        case T_SUBTRACT:
            walk_pointer_arithmetic(writtenPointer, block, scope, tacIndex, tempNum, &assignment->operands.store.address);
            break;

        default:
            walk_sub_expression(writtenPointer, block, scope, tacIndex, tempNum, &assignment->operands.store.address);
            break;
        }
        assignment->operands.store.source = assignedValue;
    }
    break;

    case T_ARRAY_INDEX:
    {
        assignment->operation = TT_ARRAY_STORE;
        switch (lhs->child->type)
        {
        case T_DOT:
        {
            struct TACLine *arrayFieldAccess = walk_field_access(lhs->child, block, scope, tacIndex, tempNum, &assignment->operands.arrayStore.array, 0);
            convert_field_load_to_lea(arrayFieldAccess, &assignment->operands.arrayStore.array);
        }
        break;

        case T_ARRAY_INDEX:
        {
            struct TACLine *arrayArrayAccess = walk_array_read(lhs->child, block, scope, tacIndex, tempNum);
            convert_array_load_to_lea(arrayArrayAccess, &assignment->operands.arrayStore.array);
        }
        break;

        default:
        {
            walk_sub_expression(lhs->child, block, scope, tacIndex, tempNum, &assignment->operands.arrayStore.array);
        }
        }
        walk_sub_expression(lhs->child->sibling, block, scope, tacIndex, tempNum, &assignment->operands.arrayStore.index);
        assignment->operands.arrayStore.source = assignedValue;
    }
    break;

    case T_DOT:
    {
        assignment->operation = TT_FIELD_STORE;
        if (lhs->child->type == T_DOT)
        {
            struct TACLine *dotRead = walk_field_access(lhs->child, block, scope, tacIndex, tempNum, &assignment->operands.fieldStore.destination, 0);
            convert_field_load_to_lea(dotRead, &assignment->operands.fieldStore.destination);
        }
        else
        {
            walk_sub_expression(lhs->child, block, scope, tacIndex, tempNum, &assignment->operands.fieldStore.destination);
        }
        // TODO: more verbose error handling if the lhs->child subexpression is not a struct, or has wrong pointer level
        struct StructDesc *writtenStruct = scope_lookup_struct_by_type_or_pointer(scope, tac_operand_get_type(&assignment->operands.fieldStore.destination));
        struct StructField *writtenField = struct_lookup_field(writtenStruct, lhs->child->sibling, scope);
        assignment->operands.fieldStore.fieldName = writtenField->variable->name;
        assignment->operands.fieldStore.source = assignedValue;
    }
    break;

    default:
        log_tree(LOG_FATAL, lhs, "Unexpected AST (%s) seen in walk_assignment!", lhs->value);
        break;
    }

    switch (assignment->operation)
    {
    case TT_ASSIGN:
        if (rhs->type == T_INITIALIZER)
        {
            walk_initializer(rhs, block, scope, tacIndex, tempNum, &assignment->operands.assign.destination);
            free(assignment);
            assignment = NULL;
        }
        else
        {
            check_assignment_operand_types(tree,
                                           tac_operand_get_type(&assignment->operands.assign.source),
                                           tac_operand_get_type(&assignment->operands.assign.destination));
        }
        break;

    case TT_STORE:
        if (rhs->type == T_INITIALIZER)
        {
            walk_initializer(rhs, block, scope, tacIndex, tempNum, &assignment->operands.store.source);
            free(assignment);
            assignment = NULL;
        }
        else
        {
            struct Type ptrType = type_duplicate_non_pointer(tac_operand_get_type(&assignment->operands.store.address));
            if (ptrType.pointerLevel > 0)
            {
                ptrType.pointerLevel--;
            }
            else
            {
                log_tree(LOG_FATAL, tree, "Non-pointer type %s seen in lhs of store operation!", type_get_name(&ptrType));
            }

            check_assignment_operand_types(tree,
                                           tac_operand_get_type(&assignment->operands.store.source),
                                           &ptrType);
            type_deinit(&ptrType);
        }
        break;

    case TT_ARRAY_STORE:
        if (rhs->type == T_INITIALIZER)
        {
            walk_initializer(rhs, block, scope, tacIndex, tempNum, &assignment->operands.arrayStore.source);
            free(assignment);
            assignment = NULL;
        }
        else
        {
            // if more deeply nested levels of linearization gave us a pointer to an array, this is valid in the TAC
            // but the assignment type check will fail, so we need to strip down a pointer level from the array type
            struct Type arrayType = type_duplicate_non_pointer(tac_operand_get_type(&assignment->operands.arrayStore.array));
            if (arrayType.pointerLevel > 0)
            {
                arrayType.pointerLevel--;
            }
            check_assignment_operand_types(tree,
                                           tac_operand_get_type(&assignment->operands.arrayStore.source),
                                           &arrayType);
            type_deinit(&arrayType);
        }
        break;

    case TT_FIELD_STORE:
        if (rhs->type == T_INITIALIZER)
        {
            walk_initializer(rhs, block, scope, tacIndex, tempNum, &assignment->operands.fieldStore.source);
            free(assignment);
            assignment = NULL;
        }
        else
        {
            struct StructDesc *writtenStruct = scope_lookup_struct_by_type_or_pointer(scope, tac_operand_get_type(&assignment->operands.fieldStore.destination));
            struct StructField *writtenField = struct_lookup_field(writtenStruct, lhs->child->sibling, scope);
            check_assignment_operand_types(tree,
                                           tac_operand_get_type(&assignment->operands.fieldStore.source),
                                           &writtenField->variable->type);
        }
        break;

    default:
        log_tree(LOG_FATAL, tree, "Unexpected assignment operation (%s) seen in walk_assignment!", tac_operation_get_name(assignment->operation));
    }

    if (assignment != NULL)
    {
        basic_block_append(block, assignment, tacIndex);
    }
}

void walk_arithmetic_assignment(struct Ast *tree,
                                struct BasicBlock *block,
                                struct Scope *scope,
                                size_t *tacIndex,
                                size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_arithmetic_assignment");

    struct Ast fakeArith = *tree;
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
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_arithmetic_assignment!", token_get_name(tree->type));
    }

    // our fake arithmetic ast will have the child of the arithmetic assignment operator
    // this effectively duplicates the LHS of the assignment to the first operand of the arithmetic operator
    struct Ast *lhs = tree->child;
    fakeArith.child = lhs;

    struct Ast fakelhs = *lhs;
    fakelhs.sibling = &fakeArith;

    struct Ast fakeAssignment = *tree;
    fakeAssignment.value = "=";
    fakeAssignment.type = T_ASSIGN;

    fakeAssignment.child = &fakelhs;

    walk_assignment(&fakeAssignment, block, scope, tacIndex, tempNum);
}

struct TACOperand *walk_bitwise_not(struct Ast *tree,
                                    struct BasicBlock *block,
                                    struct Scope *scope,
                                    size_t *tacIndex,
                                    size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "WalkBitwiseNot");

    if (tree->type != T_BITWISE_NOT)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to WalkBitwiseNot!", token_get_name(tree->type));
    }

    // generically set to TT_ADD, we will actually set the operation within switch cases
    struct TACLine *bitwiseNotLine = new_tac_line(TT_BITWISE_NOT, tree);

    walk_sub_expression(tree->child, block, scope, tacIndex, tempNum, &bitwiseNotLine->operands.arithmetic.sourceA);
    tac_operand_populate_as_temp(scope, &bitwiseNotLine->operands.arithmetic.sourceA, tempNum, tac_operand_get_type(&bitwiseNotLine->operands.arithmetic.sourceA));

    struct Type *operandAType = tac_operand_get_type(&bitwiseNotLine->operands.arithmetic.sourceA);

    // TODO: consistent bitwise arithmetic checking, print type name
    if ((operandAType->pointerLevel > 0) || (operandAType->basicType == VT_ARRAY))
    {
        log_tree(LOG_FATAL, tree, "Bitwise arithmetic on pointers is not allowed!");
    }

    basic_block_append(block, bitwiseNotLine, tacIndex);

    return &bitwiseNotLine->operands.arithmetic.destination;
}

void ensure_all_fields_initialized(struct Ast *tree, size_t initFieldIdx, struct StructDesc *initializedStruct)
{
    // TODO: fix direct data access of stack
    // if all fields of the struct are not initialized, this is an error
    if (initFieldIdx < initializedStruct->fieldLocations->size)
    {
        char *fieldsString = malloc(1);
        fieldsString[0] = '\0';

        // go through the remaining fields, construct a string with the type and name of all missing fields
        while (initFieldIdx < initializedStruct->fieldLocations->size)
        {
            struct StructField *unInitField = deque_at(initializedStruct->fieldLocations, initFieldIdx);

            char *unInitTypeName = type_get_name(&unInitField->variable->type);
            size_t origLen = strlen(fieldsString);
            size_t addlSize = strlen(unInitTypeName) + strlen(unInitField->variable->name) + 2;
            char *separatorString = "";
            if (initFieldIdx + 1 < initializedStruct->fieldLocations->size)
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

            initFieldIdx++;
        }

        log_tree(LOG_FATAL, tree, "Missing initializers for fields(s) of %s: %s", initializedStruct->name, fieldsString);
    }
}

void walk_struct_initializer(struct Ast *tree,
                             struct BasicBlock *block,
                             struct Scope *scope,
                             size_t *tacIndex,
                             size_t *tempNum,
                             struct TACOperand *initializedOperand,
                             struct Type *initializedType,
                             struct StructDesc *initializedStruct)
{
    size_t initFieldIdx = 0;
    for (struct Ast *initRunner = tree; initRunner != NULL; initRunner = initRunner->sibling)
    {
        // sanity check initializer parse
        if (initRunner->type != T_ASSIGN)
        {
            InternalError("Malformed AST seen inside struct initializer, expected T_ASSIGN, with first child as T_IDENTIFIER, got %s with first child as %s", token_get_name(initRunner->type));
        }

        struct Ast *initFieldTree = initRunner->child;
        struct Ast *initToTree = initFieldTree->sibling;

        if (initFieldTree->type != T_IDENTIFIER)
        {
            InternalError("Malformed AST for initializer, expected identifier on LHS but got %s", token_get_name(initFieldTree->type));
        }

        // first, attempt to look up the field by tree in order to throw an error in the case of a nonexistent one being referenced
        struct StructField *initializedField = struct_lookup_field(initializedStruct, initFieldTree, scope);

        // next, check the ordering index for the field we are expecting to initialize
        struct StructField *expectedField = deque_at(initializedStruct->fieldLocations, initFieldIdx);
        if ((initializedField->offset != expectedField->offset) || (strcmp(initializedField->variable->name, expectedField->variable->name) != 0))
        {
            log(LOG_FATAL, "Initializer element %zu of struct %s should be %s, not %s", initFieldIdx + 1, initializedStruct->name, expectedField->variable->name, initializedField->variable->name);
        }

        struct TACOperand initializedValue = {0};

        struct TACLine *fieldStore = new_tac_line(TT_FIELD_STORE, initRunner);
        fieldStore->operands.fieldStore.destination = *initializedOperand;
        fieldStore->operands.fieldStore.fieldName = initializedField->variable->name;

        if (initToTree->type == T_INITIALIZER)
        {
            // we are initializing the field directly from its address, recurse
            tac_operand_populate_as_temp(scope, &fieldStore->operands.fieldStore.source, tempNum, &initializedField->variable->type);
            walk_initializer(initToTree, block, scope, tacIndex, tempNum, &fieldStore->operands.fieldStore.source);
        }
        else
        {
            walk_sub_expression(initToTree, block, scope, tacIndex, tempNum, &fieldStore->operands.fieldStore.source);

            // make sure the subexpression has a sane type to be stored in the field we are initializing
            if (type_compare_allow_implicit_widening(tac_operand_get_type(&fieldStore->operands.fieldStore.source), &initializedField->variable->type))
            {
                log_tree(LOG_FATAL, initToTree, "Initializer expression for field %s.%s has type %s but expected type %s", initializedStruct->name, initializedField->variable->name, type_get_name(tac_operand_get_type(&initializedValue)), type_get_name(&initializedField->variable->type));
            }
        }
        basic_block_append(block, fieldStore, tacIndex);

        initFieldIdx++;
    }
    ensure_all_fields_initialized(tree, initFieldIdx, initializedStruct);
}

void walk_enum_initializer(struct Ast *tree,
                           struct Ast *initializerTree,
                           struct BasicBlock *block,
                           struct Scope *scope,
                           size_t *tacIndex,
                           size_t *tempNum,
                           struct TACOperand *initializedOperand,
                           struct EnumMember *fromMember)
{
    struct Type *enumType = tac_operand_get_type(initializedOperand);
    struct TypeEntry *enumTypeEntry = scope_lookup_type_remove_pointer(scope, enumType);
    struct EnumDesc *fromEnum = enumTypeEntry->data.asEnum;

    type_init(&initializedOperand->castAsType);

    // we're going to have a temp
    if (tac_operand_get_type(initializedOperand)->basicType == VT_NULL)
    {
        tac_operand_populate_as_temp(scope, initializedOperand, tempNum, enumType);
    }

    struct TACOperand *destAddr = initializedOperand;
    if (tac_operand_get_type(initializedOperand)->pointerLevel == 0)
    {
        // we will manipulate based on the address of the enum
        destAddr = get_addr_of_operand(initializerTree, block, scope, tacIndex, tempNum, initializedOperand);
    }

    // the first sizeof(size_t) bytes of this enum are the numerical index of the member with which we are dealing
    struct TACLine *writeEnumNumericalLine = new_tac_line(TT_STORE, initializerTree);
    writeEnumNumericalLine->operands.store.address = *destAddr;

    writeEnumNumericalLine->operands.store.source.name.val = fromMember->numerical;
    writeEnumNumericalLine->operands.store.source.castAsType.basicType = select_variable_type_for_number(fromMember->numerical); // TODO: define for size_t
    writeEnumNumericalLine->operands.store.source.permutation = VP_LITERAL_VAL;

    // treat our enum dest addr as actually a pointer to the numerical index of what member it is
    writeEnumNumericalLine->operands.store.address.castAsType = *tac_operand_get_type(&writeEnumNumericalLine->operands.store.source);
    writeEnumNumericalLine->operands.store.address.castAsType.pointerLevel++;
    basic_block_append(block, writeEnumNumericalLine, tacIndex);

    // if there is some sort of initializer for the data tagged to this enum member
    if (tree != NULL)
    {
        // make sure that the enum member actually expects data
        if (fromMember->type.basicType == VT_NULL)
        {
            log_tree(LOG_FATAL, initializerTree, "Attempt to populate data of enum %s member %s, which expects no data", fromEnum->name, fromMember->name);
        }

        // compute the address where the actual data lives in this enum object (base address + sizeof(size_t))
        struct TACLine *enumDataAddrCompLine = new_tac_line(TT_ADD, initializerTree);

        struct Type pointerToFromMemberType = fromMember->type;
        pointerToFromMemberType.pointerLevel++;
        tac_operand_populate_as_temp(scope, &enumDataAddrCompLine->operands.arithmetic.destination, tempNum, &pointerToFromMemberType);

        enumDataAddrCompLine->operands.arithmetic.sourceA = *destAddr;

        enumDataAddrCompLine->operands.arithmetic.sourceB.permutation = VP_LITERAL_VAL;
        enumDataAddrCompLine->operands.arithmetic.sourceB.name.val = sizeof(size_t);
        enumDataAddrCompLine->operands.arithmetic.sourceB.castAsType.basicType = select_variable_type_for_number(sizeof(size_t));

        basic_block_append(block, enumDataAddrCompLine, tacIndex);

        if (type_is_struct_object(&fromMember->type))
        {
            // log_tree(LOG_FATAL, tree->child, "Cannot initialize struct object %s in enum %s member %s", type_get_name(&fromMember->type), fromEnum->name, fromMember->name);
            struct StructDesc *fromStruct = scope_lookup_struct_by_type(scope, &fromMember->type);
            walk_struct_initializer(tree, block, scope, tacIndex, tempNum, &enumDataAddrCompLine->operands.arithmetic.destination, &fromMember->type, fromStruct);
        }
        else
        {
            struct TACLine *enumDataAssignLine = new_tac_line(TT_STORE, tree);
            enumDataAssignLine->operands.store.address = enumDataAddrCompLine->operands.arithmetic.destination;
            enumDataAssignLine->operands.store.address.castAsType = fromMember->type;
            enumDataAssignLine->operands.store.address.castAsType.pointerLevel++;

            walk_sub_expression(tree, block, scope, tacIndex, tempNum, &enumDataAssignLine->operands.store.source);
            struct Type *subExprDataType = tac_operand_get_type(&enumDataAssignLine->operands.store.source);

            if (type_compare_allow_implicit_widening(subExprDataType, &fromMember->type))
            {
                log_tree(LOG_FATAL, tree, "Invalid assignment of data from enum %s member %s (expect type %s) to type %s",
                         fromEnum->name,
                         fromMember->name,
                         type_get_name(&fromMember->type),
                         type_get_name(subExprDataType));
            }
            basic_block_append(block, enumDataAssignLine, tacIndex);
        }
    }
    else // no data tagged to this enum member
    {
        // make sure that the enum member doesn't expect any data, error otherwise
        if (fromMember->type.basicType != VT_NULL)
        {
            log_tree(LOG_FATAL, initializerTree, "Unpopulated data of enum %s member %s, which expects data of type %s", fromEnum->name, fromMember->name, type_get_name(&fromMember->type));
        }
    }
}

void walk_initializer(struct Ast *tree,
                      struct BasicBlock *block,
                      struct Scope *scope,
                      size_t *tacIndex,
                      size_t *tempNum,
                      struct TACOperand *initialized)
{
    if (tree->type != T_INITIALIZER)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_initializer!", token_get_name(tree->type));
    }

    struct Ast *initializedTypeNameTree = tree->child;
    struct Type initializedType = {0};

    struct TypeEntry *implFor = NULL;
    if (scope->parentFunction != NULL)
    {
        implFor = scope->parentFunction->implementedFor;
    }

    walk_type_name(initializedTypeNameTree, scope, &initializedType, implFor);
    struct TypeEntry *initializedTypeEntry = scope_lookup_type(scope, &initializedType);

    // make sure we initialize only a struct/enum or a * if we already know the type of what we should be initializing
    if (tac_operand_get_type(initialized)->basicType != VT_NULL)
    {
        struct Type *intendedType = type_duplicate(tac_operand_get_type(initialized));
        if (intendedType->basicType == VT_SELF)
        {
            intendedType->basicType = initializedTypeEntry->type.basicType;
            intendedType->nonArray.complexType.name = initializedTypeEntry->baseName;
            if (initializedTypeEntry->genericType == G_INSTANCE)
            {
                intendedType->nonArray.complexType.genericParams = initializedTypeEntry->generic.instance.parameters;
            }
        }

        if (!type_is_struct_object(intendedType) && !((intendedType->basicType == VT_STRUCT) && (intendedType->pointerLevel == 1)) &&
            !type_is_enum_object(intendedType) && !((intendedType->basicType == VT_ENUM) && (intendedType->pointerLevel == 1)))
        {
            log_tree(LOG_FATAL, tree, "Cannot use initializer type %s which is neither a struct or an enum", type_get_name(intendedType));
        }

        if (type_compare(&initializedTypeEntry->type, intendedType) != 0)
        {
            log_tree(LOG_FATAL, tree, "Initializer type for struct %s does not match declared type %s", type_get_name(&initializedTypeEntry->type), type_get_name(intendedType));
        }
        type_free(intendedType);
    }
    else
    {
        tac_operand_populate_as_temp(scope, initialized, tempNum, &initializedTypeEntry->type);
    }

    // automagically get the address of whatever we are initializing if it is a regular struct
    // TODO: test initializing pointers directly? Is this desirable behavior like allowing struct.field for both structs and struct*s or is this nonsense?
    if (type_is_object(tac_operand_get_type(initialized)))
    {
        initialized = get_addr_of_operand(tree, block, scope, tacIndex, tempNum, initialized);
    }

    switch (initializedTypeEntry->permutation)
    {
    case TP_PRIMITIVE:
        log_tree(LOG_FATAL, tree, "Cannot initialize a primitive type");

    case TP_STRUCT:
    {
        struct Ast *memberInitializers = tree->child->sibling;
        walk_struct_initializer(memberInitializers, block, scope, tacIndex, tempNum, initialized, &initializedType, initializedTypeEntry->data.asStruct);
    }
    break;

    case TP_ENUM:
    {
        if ((initializedTypeNameTree->child->type != T_IDENTIFIER) && (initializedTypeNameTree->child->type != T_CAP_SELF))
        {
            log_tree(LOG_FATAL, initializedTypeNameTree, "Expected identifier for enum type name, got %s", token_get_name(initializedTypeNameTree->child->type));
        }
        struct Ast *initializedMemberTree = initializedTypeNameTree->sibling;
        struct Ast *memberInitializers = initializedMemberTree->sibling;
        struct EnumMember *initializedMember = enum_lookup_member(initializedTypeEntry->data.asEnum, initializedMemberTree);

        walk_enum_initializer(memberInitializers, tree, block, scope, tacIndex, tempNum, initialized, initializedMember);
    }
    break;
    }
}

void walk_sub_expression(struct Ast *tree,
                         struct BasicBlock *block,
                         struct Scope *scope,
                         size_t *tacIndex,
                         size_t *tempNum,
                         struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_sub_expression");

    switch (tree->type)
    {
        // variable read
    case T_SELF:
    {
        struct VariableEntry *readVariable = scope_lookup_var(scope, tree);
        tac_operand_populate_from_variable(destinationOperand, readVariable);
    }
    break;

    // identifier = variable name
    case T_IDENTIFIER:
    {
        struct VariableEntry *readVariable = scope_lookup_var(scope, tree);
        tac_operand_populate_from_variable(destinationOperand, readVariable);
    }
    break;

    // FIXME: there exists some code path where we can reach this point with garbage in types, resulting in a crash when printing TAC operand types
    case T_CONSTANT:
        type_init(&destinationOperand->castAsType);
        destinationOperand->name.str = tree->value;
        destinationOperand->castAsType.basicType = select_variable_type_for_literal(tree->value);
        destinationOperand->permutation = VP_LITERAL_STR;
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
        destinationOperand->castAsType.basicType = VT_U8;
        destinationOperand->permutation = VP_LITERAL_STR;
    }
    break;

    case T_STRING_LITERAL:
        walk_string_literal(tree, block, scope, destinationOperand);
        break;

    case T_FUNCTION_CALL:
    {
        walk_function_call(tree, block, scope, tacIndex, tempNum, destinationOperand);
    }
    break;

    case T_METHOD_CALL:
    {
        walk_method_call(tree, block, scope, tacIndex, tempNum, destinationOperand);
    }
    break;

    case T_ASSOCIATED_CALL:
    {
        walk_associated_call(tree, block, scope, tacIndex, tempNum, destinationOperand);
    }
    break;

    case T_DOT:
    {
        walk_field_access(tree, block, scope, tacIndex, tempNum, destinationOperand, 0);
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
        struct TACOperand *expressionResult = walk_expression(tree, block, scope, tacIndex, tempNum);
        *destinationOperand = *expressionResult;
    }
    break;

    case T_BITWISE_NOT:
    {
        struct TACOperand *bitwiseNotResult = walk_bitwise_not(tree, block, scope, tacIndex, tempNum);
        *destinationOperand = *bitwiseNotResult;
    }
    break;

    // array reference
    case T_ARRAY_INDEX:
    {
        struct TACLine *arrayRefLine = walk_array_read(tree, block, scope, tacIndex, tempNum);
        *destinationOperand = arrayRefLine->operands.arrayLoad.destination;
        if (type_is_object(tac_operand_get_type(&arrayRefLine->operands.arrayLoad.destination)))
        {
            convert_array_load_to_lea(arrayRefLine, destinationOperand);
        }
    }
    break;

    case T_DEREFERENCE:
    {
        struct TACOperand *dereferenceResult = walk_dereference(tree, block, scope, tacIndex, tempNum);
        *destinationOperand = *dereferenceResult;
    }
    break;

    case T_ADDRESS_OF:
    {
        struct TACOperand *addrOfResult = walk_addr_of(tree, block, scope, tacIndex, tempNum);
        *destinationOperand = *addrOfResult;
    }
    break;

    case T_BITWISE_AND:
    {
        struct TACOperand *expressionResult = walk_expression(tree, block, scope, tacIndex, tempNum);
        *destinationOperand = *expressionResult;
    }
    break;

    // TODO: helper function for casting - can better enforce validity of casting with true array types
    case T_CAST:
    {
        struct TACOperand expressionResult = {0};

        // Walk the right child of the cast, the subexpression we are casting
        walk_sub_expression(tree->child->sibling, block, scope, tacIndex, tempNum, &expressionResult);

        struct Type castTo;
        type_init(&castTo);
        // set the result's cast as type based on the child of the cast, the type we are casting to
        walk_type_name(tree->child, scope, &castTo, NULL);

        // TODO: allow casting to arrays?
        if (type_is_object(&expressionResult.castAsType))
        {
            char *castToType = type_get_name(&expressionResult.castAsType);
            log_tree(LOG_FATAL, tree->child, "Casting to an object (%s) is not allowed!", castToType);
        }

        struct Type *castFrom = tac_operand_get_non_cast_type(&expressionResult);

        // If necessary, lop bits off the big end of the value with an explicit bitwise and operation, storing to an intermediate temp
        if (type_compare_allow_implicit_widening(castFrom, &castTo) && (castTo.pointerLevel == 0))
        {
            struct TACLine *castBitManipulation = new_tac_line(TT_BITWISE_AND, tree);

            // RHS of the assignment is whatever we are storing, what is being cast
            castBitManipulation->operands.arithmetic.sourceA = expressionResult;

            // construct the bit pattern we will use in order to properly mask off the extra bits (TODO: will not hold for unsigned types)
            // TODO: rectify to use VP_LITERAL_VAL
            castBitManipulation->operands.arithmetic.sourceB.permutation = VP_LITERAL_VAL;
            castBitManipulation->operands.arithmetic.sourceB.castAsType.basicType = VT_U64; // TODO: define for size_t type

            size_t castToWidth = type_get_size(&castTo, scope);
            switch (castToWidth)
            {
            case sizeof(u8):
                castBitManipulation->operands.arithmetic.sourceB.name.val = U8_MAX;
                break;
            case sizeof(u16):
                castBitManipulation->operands.arithmetic.sourceB.name.val = U16_MAX;
                break;
            case sizeof(u32):
                castBitManipulation->operands.arithmetic.sourceB.name.val = U32_MAX;
                break;
            case sizeof(u64):
                castBitManipulation->operands.arithmetic.sourceB.name.val = U64_MAX;
                break;
            default:
                InternalError("Type case to size not equal to any integral type size (%zu)!", castToWidth);
            }

            // destination of our bit manipulation is a temporary variable with the type to which we are casting
            // TODO: this pattern is repeated anywhere arithmetic temps are dealt with, consider a helper function
            tac_operand_populate_as_temp(scope, &castBitManipulation->operands.arithmetic.destination, tempNum, tac_operand_get_type(&castBitManipulation->operands.arithmetic.sourceA));

            // attach our bit manipulation operation to the end of the basic block
            basic_block_append(block, castBitManipulation, tacIndex);
            // set the destination operation of this subexpression to read the manipulated value we just wrote
            castBitManipulation->operands.arithmetic.destination.castAsType = castTo;
            *destinationOperand = castBitManipulation->operands.arithmetic.destination;
        }
        else
        {
            // no bit manipulation required, simply set the destination operand to the result of the casted subexpression (with cast as type set by us)
            expressionResult.castAsType = castTo;
            *destinationOperand = expressionResult;
        }
    }
    break;

    case T_SIZEOF:
        walk_sizeof(tree, block, scope, tacIndex, tempNum, destinationOperand);
        break;

    case T_INITIALIZER:
        walk_initializer(tree, block, scope, tacIndex, tempNum, destinationOperand);
        break;

    default:
        log_tree(LOG_FATAL, tree, "Incorrect AST type (%s) seen while linearizing subexpression!", token_get_name(tree->type));
        break;
    }
}

void check_function_return_use(struct Ast *tree,
                               struct TACOperand *destinationOperand,
                               struct FunctionEntry *calledFunction)
{
    if ((destinationOperand != NULL) &&
        (calledFunction->returnType.basicType == VT_NULL))
    {
        log_tree(LOG_FATAL, tree, "Attempt to use return value of function %s which does not return anything!", calledFunction->name);
    }
}

Deque *walk_argument_pushes(struct Ast *argumentRunner,
                            struct FunctionEntry *calledFunction,
                            struct BasicBlock *block,
                            struct Scope *scope,
                            size_t *tacIndex,
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
    struct Ast *lastArgument = argumentRunner;
    Deque *argumentPushes = deque_new(NULL);

    Deque *argumentTrees = deque_new(NULL);
    while (argumentRunner != NULL)
    {
        deque_push_back(argumentTrees, argumentRunner);
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

    Iterator *calledFunctionArgumentIterator = deque_front(calledFunction->arguments);

    // account for offsetting the number of arguments for self and out pointers
    while (argumentNumOffset > 0)
    {
        iterator_next(calledFunctionArgumentIterator);
        argumentNumOffset--;
    }

    while (argumentTrees->size > 0)
    {
        struct Ast *pushedArgument = deque_pop_front(argumentTrees);
        struct TACOperand *argOperand = malloc(sizeof(struct TACOperand));
        memset(argOperand, 0, sizeof(struct TACOperand));
        deque_push_back(argumentPushes, argOperand);
        walk_sub_expression(pushedArgument, block, scope, tacIndex, tempNum, argOperand);

        struct VariableEntry *expectedArgument = iterator_get(calledFunctionArgumentIterator);
        iterator_next(calledFunctionArgumentIterator);

        if (type_compare_allow_implicit_widening(tac_operand_get_type(argOperand), &expectedArgument->type))
        {
            log_tree(LOG_FATAL, pushedArgument,
                     "Error in argument %s passed to function %s!\n\tExpected %s, got %s",
                     expectedArgument->name,
                     calledFunction->name,
                     type_get_name(&expectedArgument->type),
                     type_get_name(tac_operand_get_type(argOperand)));
        }

        // allow us to automatically widen
        if (type_get_size(tac_operand_get_type(argOperand), scope) <= type_get_size(&expectedArgument->type, scope))
        {
            argOperand->castAsType = expectedArgument->type;
        }
        else
        {
            char *convertFromType = type_get_name(tac_operand_get_type(argOperand));
            char *convertToType = type_get_name(&expectedArgument->type);
            log_tree(LOG_FATAL, pushedArgument,
                     "Potential narrowing conversion passed to argument %s of function %s\n\tConversion from %s to %s",
                     expectedArgument->name,
                     calledFunction->name,
                     convertFromType,
                     convertToType);
        }
    }
    iterator_free(calledFunctionArgumentIterator);

    deque_free(argumentTrees);

    return argumentPushes;
}

// returns true if the function returns a struct and the return value has been handled, false otherwise
bool handle_struct_return(struct Ast *callTree,
                          struct FunctionEntry *calledFunction,
                          struct BasicBlock *block,
                          struct Scope *scope,
                          size_t *tacIndex,
                          size_t *tempNum,
                          Deque *argumentPushes,
                          struct TACOperand *destinationOperand)
{
    if (!type_is_object(&calledFunction->returnType))
    {
        return false;
    }

    log(LOG_DEBUG, "handleStructReturn for called function %s", calledFunction->name);

    struct TACOperand *outPointerArg = malloc(sizeof(struct TACOperand));
    memset(outPointerArg, 0, sizeof(struct TACOperand));

    // if we actually use the return value of the function
    if (destinationOperand != NULL)
    {
        struct TACOperand intermediateReturnObject = {0};
        tac_operand_populate_as_temp(scope, &intermediateReturnObject, tempNum, &calledFunction->returnType);
        log_tree(LOG_DEBUG, callTree, "Call to %s returns struct in %s", calledFunction->name, intermediateReturnObject.name.str);

        *destinationOperand = intermediateReturnObject;
        struct TACOperand *addrOfReturnObject = get_addr_of_operand(callTree, block, scope, tacIndex, tempNum, &intermediateReturnObject);

        *outPointerArg = *addrOfReturnObject;
    }
    else
    {
        log(LOG_FATAL, "Unused return value for function %s returning %s", calledFunction->name, type_get_name(&calledFunction->returnType));
    }
    deque_push_front(argumentPushes, outPointerArg);

    return true;
}

void walk_function_call(struct Ast *tree,
                        struct BasicBlock *block,
                        struct Scope *scope,
                        size_t *tacIndex,
                        size_t *tempNum,
                        struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_function_call");

    if (tree->type != T_FUNCTION_CALL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_function_call!", token_get_name(tree->type));
    }

    scope->parentFunction->callsOtherFunction = 1;

    struct FunctionEntry *calledFunction = scope_lookup_fun(scope, tree->child);

    check_function_return_use(tree, destinationOperand, calledFunction);

    Deque *argumentPushes = walk_argument_pushes(tree->child->sibling,
                                                 calledFunction,
                                                 block,
                                                 scope,
                                                 tacIndex,
                                                 tempNum,
                                                 destinationOperand);

    bool haveStructReturn = handle_struct_return(tree, calledFunction, block, scope, tacIndex, tempNum, argumentPushes, destinationOperand);

    struct TACLine *callLine = new_tac_line(TT_FUNCTION_CALL, tree);
    if (!haveStructReturn && (destinationOperand != NULL))
    {
        tac_operand_populate_as_temp(scope, destinationOperand, tempNum, &calledFunction->returnType);
        callLine->operands.functionCall.returnValue = *destinationOperand;
    }
    callLine->operands.functionCall.functionName = calledFunction->name;
    callLine->operands.functionCall.arguments = argumentPushes;
    basic_block_append(block, callLine, tacIndex);
}

void walk_method_call(struct Ast *tree,
                      struct BasicBlock *block,
                      struct Scope *scope,
                      size_t *tacIndex,
                      size_t *tempNum,
                      struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_method_call");

    if (tree->type != T_METHOD_CALL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_method_call!", token_get_name(tree->type));
    }

    scope->parentFunction->callsOtherFunction = 1;

    // don't need to track scope->parentFunction->callsOtherFunction as walk_function_call will do this on our behalf
    struct Ast *structTree = tree->child->child;
    struct Ast *callTree = tree->child->child->sibling;

    struct TACOperand *calledOnOperand = malloc(sizeof(struct TACOperand));

    switch (structTree->type)
    {
        // if we have struct.field.method() make sure we convert the struct.field load to an LEA
    case T_DOT:
    {
        struct TACLine *fieldAccessLine = walk_field_access(structTree, block, scope, tacIndex, tempNum, calledOnOperand, 0);
        convert_field_load_to_lea(fieldAccessLine, calledOnOperand);
    }
    break;

    default:
    {
        walk_sub_expression(structTree, block, scope, tacIndex, tempNum, calledOnOperand);
        struct Type *structType = tac_operand_get_type(calledOnOperand);
        if ((structType->basicType != VT_STRUCT) && (structType->basicType != VT_SELF))
        {
            char *nonStructType = type_get_name(tac_operand_get_type(calledOnOperand));
            log_tree(LOG_FATAL, structTree, "Attempt to call method %s on non-struct type %s", callTree->child->value, nonStructType);
        }
    }
    break;
    }

    struct FunctionEntry *calledFunction = type_entry_lookup_implemented(scope_lookup_type_remove_pointer(scope, tac_operand_get_type(calledOnOperand)), scope, callTree->child);

    check_function_return_use(tree, destinationOperand, calledFunction);

    Deque *argumentPushes = walk_argument_pushes(tree->child->child->sibling->child->sibling,
                                                 calledFunction,
                                                 block,
                                                 scope,
                                                 tacIndex,
                                                 tempNum,
                                                 destinationOperand);

    if (tac_operand_get_type(calledOnOperand)->basicType == VT_ARRAY)
    {
        char *nonDottableType = type_get_name(tac_operand_get_type(calledOnOperand));
        log_tree(LOG_FATAL, callTree, "Attempt to call method %s on non-dottable type %s", calledFunction->name, nonDottableType);
    }

    // if struct we are calling method on is not indirect, automagically insert an intermediate address-of
    if (tac_operand_get_type(calledOnOperand)->pointerLevel == 0)
    {
        *calledOnOperand = *get_addr_of_operand(tree, block, scope, tacIndex, tempNum, calledOnOperand);
    }

    deque_push_front(argumentPushes, calledOnOperand);

    bool haveStructReturn = handle_struct_return(tree, calledFunction, block, scope, tacIndex, tempNum, argumentPushes, destinationOperand);

    struct TACLine *callLine = new_tac_line(TT_METHOD_CALL, tree);
    if (!haveStructReturn && (destinationOperand != NULL))
    {
        tac_operand_populate_as_temp(scope, destinationOperand, tempNum, &calledFunction->returnType);
        callLine->operands.methodCall.returnValue = *destinationOperand;
    }
    callLine->operands.methodCall.calledOn = *calledOnOperand;
    callLine->operands.methodCall.methodName = calledFunction->name;
    callLine->operands.methodCall.arguments = argumentPushes;

    basic_block_append(block, callLine, tacIndex);
}

List *walk_generic_parameters(struct Ast *tree, struct Scope *scope)
{
    log_tree(LOG_DEBUG, tree, "walk_generic_parameters");

    if (tree->type != T_GENERIC_PARAMETERS)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_generic_parameters!", token_get_name(tree->type));
    }

    List *paramsList = list_new((void (*)(void *))type_free, NULL);

    struct Ast *paramRunner = tree->child;
    while (paramRunner != NULL)
    {
        struct Type *param = malloc(sizeof(struct Type));
        type_init(param);
        walk_type_name(paramRunner, scope, param, NULL);
        list_append(paramsList, param);

        paramRunner = paramRunner->sibling;
    }

    return paramsList;
}

struct TypeEntry *walk_type_name_or_generic_instantiation(struct Scope *scope, struct Ast *tree)
{
    log_tree(LOG_DEBUG, tree, "walk_type_name_or_generic_instantiation");

    struct TypeEntry *returnedType = NULL;
    switch (tree->type)
    {
    case T_IDENTIFIER:
        returnedType = scope_lookup_struct_by_name_tree(scope, tree);
        break;

    case T_GENERIC_INSTANCE:
    {

        struct Ast *structNameTree = tree->child;
        List *genericParams = walk_generic_parameters(tree->child->sibling, scope);
        struct TypeEntry *baseGenericType = scope_lookup_struct_by_name_tree(scope, structNameTree);
        returnedType = type_entry_get_or_create_generic_instantiation(baseGenericType, genericParams);
        if (returnedType->generic.instance.parameters != genericParams)
        {
            list_free(genericParams);
        }
    }
    break;

    default:
        log_tree(LOG_FATAL, tree, "Malformed AST (%s) seen in walk_type_name_or_generic_instantiation!", token_get_name(tree->type));
    }

    return returnedType;
}

void walk_associated_call(struct Ast *tree,
                          struct BasicBlock *block,
                          struct Scope *scope,
                          size_t *tacIndex,
                          size_t *tempNum,
                          struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_associated_call");

    if (tree->type != T_ASSOCIATED_CALL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_associated_call!", token_get_name(tree->type));
    }

    scope->parentFunction->callsOtherFunction = 1;

    // don't need to track scope->parentFunction->callsOtherFunction as walk_function_call will do this on our behalf
    struct Ast *structTypeTree = tree->child->child;
    struct TypeEntry *associatedWith = NULL;
    struct Ast *callTree = tree->child->sibling;

    associatedWith = walk_type_name_or_generic_instantiation(scope, structTypeTree);

    struct FunctionEntry *calledFunction = type_entry_lookup_associated_function(associatedWith, callTree->child, scope);

    check_function_return_use(tree, destinationOperand, calledFunction);

    Deque *argumentPushes = walk_argument_pushes(tree->child->sibling->child->sibling,
                                                 calledFunction,
                                                 block,
                                                 scope,
                                                 tacIndex,
                                                 tempNum,
                                                 destinationOperand);
    bool haveStructReturn = handle_struct_return(tree, calledFunction, block, scope, tacIndex, tempNum, argumentPushes, destinationOperand);

    struct TACLine *callLine = new_tac_line(TT_ASSOCIATED_CALL, tree);
    if (!haveStructReturn && (destinationOperand != NULL))
    {
        tac_operand_populate_as_temp(scope, destinationOperand, tempNum, &calledFunction->returnType);
        callLine->operands.associatedCall.returnValue = *destinationOperand;
    }
    callLine->operands.associatedCall.functionName = calledFunction->name;

    type_init(&callLine->operands.associatedCall.associatedWith);
    callLine->operands.associatedCall.associatedWith.basicType = VT_STRUCT;
    callLine->operands.associatedCall.associatedWith.nonArray.complexType.name = associatedWith->baseName;
    if (associatedWith->genericType == G_INSTANCE)
    {
        callLine->operands.associatedCall.associatedWith.nonArray.complexType.genericParams = associatedWith->generic.instance.parameters;
    }

    callLine->operands.associatedCall.arguments = argumentPushes;

    basic_block_append(block, callLine, tacIndex);
}

struct TACLine *walk_field_access(struct Ast *tree,
                                  struct BasicBlock *block,
                                  struct Scope *scope,
                                  size_t *tacIndex,
                                  size_t *tempNum,
                                  struct TACOperand *destinationOperand,
                                  size_t depth)
{
    log_tree(LOG_DEBUG, tree, "walk_field_access");

    if (tree->type != T_DOT)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_field_access!", token_get_name(tree->type));
    }

    struct Ast *lhs = tree->child;
    struct Ast *rhs = lhs->sibling;

    // must always have [lhs].[rhs] where lhs is some subexpression which resolves to a struct and rhs is the identifier for the struct field
    if (rhs->type != T_IDENTIFIER)
    {
        log_tree(LOG_FATAL, rhs,
                 "Expected identifier on RHS of %s operator, got %s (%s) instead!",
                 token_get_name(tree->type),
                 rhs->value,
                 token_get_name(rhs->type));
    }

    struct TACLine *accessLine = NULL;
    // the LHS of the dot is the struct instance being accessed
    // the RHS is what field we are accessing
    struct Ast *fieldTree = tree->child->sibling;

    if (fieldTree->type != T_IDENTIFIER)
    {
        log_tree(LOG_FATAL, fieldTree,
                 "Expected identifier on RHS of dot operator, got %s (%s) instead!",
                 fieldTree->value,
                 token_get_name(fieldTree->type));
    }

    // our access line is a completely new TAC line, which is a load operation with an offset, storing the load result to a temp
    accessLine = new_tac_line(TT_FIELD_LOAD, tree);

    // we may need to do some manipulation of the subexpression depending on what exactly we're dotting
    switch (lhs->type)
    {
    case T_DEREFERENCE:
    {
        // let walk_dereference do the heavy lifting for us
        struct TACOperand *dereferencedOperand = walk_dereference(lhs, block, scope, tacIndex, tempNum);

        // make sure we are generally dotting something sane
        struct Type *accessedType = tac_operand_get_type(dereferencedOperand);

        check_accessed_struct_for_dot(lhs, scope, accessedType);

        // additional check so that if we dereference a struct single-pointer we force not putting the dereference there
        // effectively, ban things like a = (*object).field where object is a Struct *
        if (accessedType->pointerLevel == 0)
        {
            char *dereferencedTypeName = type_get_name(accessedType);
            log_tree(LOG_FATAL, lhs, "Use of dereference on single-indirect type %s before dot '(*struct).field' is prohibited - just use 'struct.field' instead", dereferencedTypeName);
        }

        accessLine->operands.fieldLoad.source = *dereferencedOperand;
    }
    break;

    case T_ARRAY_INDEX:
    {
        // let walk_array_read do the heavy lifting for us
        struct TACLine *arrayRefToDot = walk_array_read(lhs, block, scope, tacIndex, tempNum);

        // before we convert our array ref to an LEA to get the address of the struct we're dotting, check to make sure everything is good
        check_accessed_struct_for_dot(tree, scope, tac_operand_get_type(&arrayRefToDot->operands.arrayLoad.destination));

        // now that we know we are dotting something valid, we will just use the array reference as an address calculation for the base of whatever we're dotting
        convert_array_load_to_lea(arrayRefToDot, &accessLine->operands.fieldLoad.source);
    }
    break;

    case T_FUNCTION_CALL:
    {
        walk_function_call(lhs, block, scope, tacIndex, tempNum, &accessLine->operands.fieldLoad.source);
    }
    break;

    case T_METHOD_CALL:
    {
        walk_method_call(lhs, block, scope, tacIndex, tempNum, &accessLine->operands.fieldLoad.source);
    }
    break;

    case T_SELF:
    case T_IDENTIFIER:
    {
        // if we are dotting an identifier, insert an address-of if it is not a pointer already
        struct VariableEntry *dottedVariable = scope_lookup_var(scope, lhs);

        if (dottedVariable->type.pointerLevel == 0)
        {
            struct TACOperand dottedOperand = {0};

            walk_sub_expression(lhs, block, scope, tacIndex, tempNum, &dottedOperand);

            if (dottedOperand.permutation != VP_TEMP)
            {
                // while this check is duplicated in the checks immediately following the switch,
                // we may be able to print more verbose error info if we are directly accessing a struct identifier, so do it here.
                check_accessed_struct_for_dot(lhs, scope, tac_operand_get_type(&dottedOperand));
            }

            accessLine->operands.fieldLoad.source = *get_addr_of_operand(lhs, block, scope, tacIndex, tempNum, &dottedOperand);
        }
        else
        {
            walk_sub_expression(lhs, block, scope, tacIndex, tempNum, &accessLine->operands.fieldLoad.source);
        }
    }
    break;

    case T_DOT:
    {
        struct TACLine *recursiveFieldAccess = walk_field_access(lhs, block, scope, tacIndex, tempNum, &accessLine->operands.fieldLoad.source, 0);
        convert_field_load_to_lea(recursiveFieldAccess, &accessLine->operands.fieldLoad.source);
    }
    break;

    default:
        log_tree(LOG_FATAL, lhs, "Dot operator field access on disallowed tree type %s", token_get_name(lhs->type));
        break;
    }

    struct Type *accessedType = tac_operand_get_type(&accessLine->operands.fieldLoad.source);
    if ((accessedType->basicType != VT_STRUCT) && (accessedType->basicType != VT_SELF))
    {
        char *accessedTypeName = type_get_name(accessedType);
        log_tree(LOG_FATAL, tree, "Use of dot operator for field access on non-struct type %s", accessedTypeName);
    }

    // get the StructDesc and StructField of what we're accessing within and the field we access
    struct StructDesc *accessedStruct = scope_lookup_struct_by_type_or_pointer(scope, accessedType);
    struct StructField *accessedField = struct_lookup_field(accessedStruct, rhs, scope);

    // populate type information (use cast for the first operand as we are treating a struct as a pointer to something else with a given offset)
    tac_operand_populate_as_temp(scope, &accessLine->operands.fieldLoad.destination, tempNum, &accessedField->variable->type);

    accessLine->operands.fieldLoad.fieldName = accessedField->variable->name;

    if (depth == 0)
    {
        basic_block_append(block, accessLine, tacIndex);
        *destinationOperand = accessLine->operands.fieldLoad.destination;
    }
    else
    {
        convert_field_load_to_lea(accessLine, destinationOperand);
    }

    return accessLine;
}

void walk_non_pointer_arithmetic(struct Ast *tree,
                                 struct BasicBlock *block,
                                 struct Scope *scope,
                                 size_t *tacIndex,
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
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to WalkNonPointerArithmetic!", token_get_name(tree->type));
        break;
    }

    walk_sub_expression(tree->child, block, scope, tacIndex, tempNum, &expression->operands.arithmetic.sourceA);
    walk_sub_expression(tree->child->sibling, block, scope, tacIndex, tempNum, &expression->operands.arithmetic.sourceB);

    struct Type *checkedType = tac_operand_get_type(&expression->operands.arithmetic.sourceA);
    if ((checkedType->pointerLevel > 0) || (checkedType->basicType == VT_ARRAY))
    {
        char *typeName = type_get_name(checkedType);
        log_tree(LOG_FATAL, tree->child, "Arithmetic operation attempted on type %s, %s is only allowed on non-indirect types", typeName, tree->value);
    }

    checkedType = tac_operand_get_type(&expression->operands.arithmetic.sourceB);
    if ((checkedType->pointerLevel > 0) || (checkedType->basicType == VT_ARRAY))
    {
        char *typeName = type_get_name(checkedType);
        log_tree(LOG_FATAL, tree->child, "Arithmetic operation attempted on type %s, %s is only allowed on non-indirect types", typeName, tree->value);
    }
}

struct TACOperand *walk_expression(struct Ast *tree,
                                   struct BasicBlock *block,
                                   struct Scope *scope,
                                   size_t *tacIndex,
                                   size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_expression");

    // generically set to TT_ADD, we will actually set the operation within switch cases
    struct TACLine *expression = new_tac_line(TT_SUBTRACT, tree);

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
        walk_non_pointer_arithmetic(tree, block, scope, tacIndex, tempNum, expression);
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

        walk_sub_expression(tree->child, block, scope, tacIndex, tempNum, &expression->operands.arithmetic.sourceA);

        // TODO: explicitly disallow arithmetic on array types?
        if (tac_operand_get_type(&expression->operands.arithmetic.sourceA)->pointerLevel > 0)
        {
            struct TACOperand offset;

            walk_sub_expression(tree->child->sibling, block, scope, tacIndex, tempNum, &offset);
            struct TACLine *scaleMultiply = set_up_scale_multiplication(tree, scope, tacIndex, tempNum, tac_operand_get_type(&expression->operands.arithmetic.sourceA), tac_operand_get_type(&offset));
            scaleMultiply->operands.arithmetic.sourceA = offset;

            tac_operand_populate_as_temp(scope, &scaleMultiply->operands.arithmetic.destination, tempNum, tac_operand_get_type(&offset));
            expression->operands.arithmetic.sourceB = scaleMultiply->operands.arithmetic.destination;

            basic_block_append(block, scaleMultiply, tacIndex);
        }
        else
        {
            walk_sub_expression(tree->child->sibling, block, scope, tacIndex, tempNum, &expression->operands.arithmetic.sourceB);
        }

        // TODO: generate errors for array types
    }
    break;

    default:
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_expression!", token_get_name(tree->type));
    }

    struct Type *operandAType = tac_operand_get_type(&expression->operands.arithmetic.sourceA);
    struct Type *operandBType = tac_operand_get_type(&expression->operands.arithmetic.sourceB);
    if ((operandAType->pointerLevel > 0) && (operandBType->pointerLevel > 0))
    {
        log_tree(LOG_FATAL, tree, "Arithmetic between 2 pointers is not allowed!");
    }

    // TODO generate errors for bad pointer arithmetic here
    if (type_get_size(operandAType, scope) > type_get_size(operandBType, scope))
    {
        tac_operand_populate_as_temp(scope, &expression->operands.arithmetic.destination, tempNum, operandAType);
    }
    else
    {
        tac_operand_populate_as_temp(scope, &expression->operands.arithmetic.destination, tempNum, operandBType);
    }

    basic_block_append(block, expression, tacIndex);

    return &expression->operands.arithmetic.destination;
}

struct TACLine *walk_array_read(struct Ast *tree,
                                struct BasicBlock *block,
                                struct Scope *scope,
                                size_t *tacIndex,
                                size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_array_read");

    if (tree->type != T_ARRAY_INDEX)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_array_read!", token_get_name(tree->type));
    }

    struct Ast *arrayBase = tree->child;
    struct Ast *arrayIndex = tree->child->sibling;

    struct TACLine *arrayRefTac = new_tac_line(TT_ARRAY_LOAD, tree);
    struct Type *arrayBaseType = NULL;

    bool subtractLeaLevel = false;

    switch (arrayBase->type)
    {
    // if the array base is an identifier, we can just look it up
    case T_IDENTIFIER:
    {
        struct VariableEntry *arrayVariable = scope_lookup_var(scope, arrayBase);
        tac_operand_populate_from_variable(&arrayRefTac->operands.arrayLoad.array, arrayVariable);
        arrayBaseType = tac_operand_get_type(&arrayRefTac->operands.arrayLoad.array);

        // sanity check - can print the name of the variable if incorrectly accessing an identifier
        // TODO: check against size of array if index is constant?
        if ((arrayBaseType->pointerLevel == 0) && (arrayBaseType->basicType != VT_ARRAY))
        {
            log_tree(LOG_FATAL, arrayBase, "Array reference on non-indirect variable %s %s", type_get_name(arrayBaseType), arrayBase->value);
        }
    }
    break;

    // FIXME: multidimensional array accesses will break in the same way that struct.arrayField[123] did before specifically checking
    case T_DOT:
    {
        struct TACLine *arrayBaseAccessLine = walk_field_access(arrayBase, block, scope, tacIndex, tempNum, &arrayRefTac->operands.arrayLoad.array, 0);
        subtractLeaLevel = convert_field_load_to_lea(arrayBaseAccessLine, &arrayBaseAccessLine->operands.arrayLoad.destination);
        arrayBaseType = tac_operand_get_type(&arrayBaseAccessLine->operands.fieldLoad.destination);
    }
    break;

    // otherwise, we need to Walk the subexpression to get the array base
    default:
    {
        walk_sub_expression(arrayBase, block, scope, tacIndex, tempNum, &arrayRefTac->operands.arrayLoad.array);
        arrayBaseType = tac_operand_get_type(&arrayRefTac->operands.arrayLoad.array);

        // sanity check - can only print the type of the base if incorrectly accessing a non-identifier through a subexpression
        if ((arrayBaseType->pointerLevel == 0) && (arrayBaseType->basicType != VT_ARRAY))
        {
            log_tree(LOG_FATAL, arrayBase, "Array reference on non-indirect type %s", type_get_name(arrayBaseType));
        }
    }
    break;
    }

    struct Type arrayMemberType = *tac_operand_get_type(&arrayRefTac->operands.arrayLoad.array);

    if ((!type_is_array_object(arrayBaseType)) && (arrayBaseType->pointerLevel == 0))
    {
        InternalError("Array-referenced type has non-indirect type of %s", type_get_name(tac_operand_get_type(&arrayRefTac->operands.arrayLoad.array)));
    }

    if (subtractLeaLevel)
    {
        arrayMemberType.pointerLevel--;
    }

    if (arrayMemberType.pointerLevel == 0)
    {
        type_single_decay(&arrayMemberType);
    }
    arrayMemberType.pointerLevel--;
    tac_operand_populate_as_temp(scope, &arrayRefTac->operands.arrayLoad.destination, tempNum, &arrayMemberType);

    walk_sub_expression(arrayIndex, block, scope, tacIndex, tempNum, &arrayRefTac->operands.arrayLoad.index);

    tac_operand_populate_as_temp(scope, &arrayRefTac->operands.arrayLoad.destination, tempNum, &arrayMemberType);

    basic_block_append(block, arrayRefTac, tacIndex);
    return arrayRefTac;
}

struct TACOperand *walk_dereference(struct Ast *tree,
                                    struct BasicBlock *block,
                                    struct Scope *scope,
                                    size_t *tacIndex,
                                    size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_dereference");

    if (tree->type != T_DEREFERENCE)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_dereference!", token_get_name(tree->type));
    }

    struct TACLine *dereference = new_tac_line(TT_LOAD, tree);

    switch (tree->child->type)
    {
    case T_ADD:
    case T_SUBTRACT:
    {
        walk_pointer_arithmetic(tree->child, block, scope, tacIndex, tempNum, &dereference->operands.load.address);
    }
    break;

    default:
        walk_sub_expression(tree->child, block, scope, tacIndex, tempNum, &dereference->operands.load.address);
        break;
    }

    struct Type *dereferencedType = tac_operand_get_type(&dereference->operands.load.address);
    if (type_is_array_object(dereferencedType))
    {
        log_tree(LOG_FATAL, tree, "Dereferencing array of type %s is not allowed!", type_get_name(dereferencedType));
    }
    else if (dereferencedType->pointerLevel == 0)
    {
        log_tree(LOG_FATAL, tree, "Dereference on non-pointer type %s is not allowed!", type_get_name(dereferencedType));
    }

    struct Type typeAfterDereference = *tac_operand_get_type(&dereference->operands.load.address);
    typeAfterDereference.pointerLevel--;
    tac_operand_populate_as_temp(scope, &dereference->operands.load.destination, tempNum, &typeAfterDereference);

    basic_block_append(block, dereference, tacIndex);

    return &dereference->operands.load.destination;
}

struct TACOperand *walk_addr_of(struct Ast *tree,
                                struct BasicBlock *block,
                                struct Scope *scope,
                                size_t *tacIndex,
                                size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_addr_of");

    if (tree->type != T_ADDRESS_OF)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to WalkAddressOf!", token_get_name(tree->type));
    }

    // TODO: helper function for getting address of
    struct TACLine *addrOfLine = new_tac_line(TT_ADDROF, tree);

    switch (tree->child->type)
    {
    // look up the variable entry for what address is taken of to generate verbose errors if it doesn't exist
    case T_IDENTIFIER:
    {
        struct VariableEntry *addrTakenOf = scope_lookup_var(scope, tree->child);
        if (addrTakenOf->type.basicType == VT_ARRAY)
        {
            log_tree(LOG_FATAL, tree->child, "Can't take address of local array %s!", addrTakenOf->name);
        }
        walk_sub_expression(tree->child, block, scope, tacIndex, tempNum, &addrOfLine->operands.addrof.source);
    }
    break;

    case T_ARRAY_INDEX:
    {
        // use walk_array_read to generate the access we need, just the direct accessing load to an lea to calculate the address we would have loaded from
        struct TACLine *arrayRefLine = walk_array_read(tree->child, block, scope, tacIndex, tempNum);
        convert_array_load_to_lea(arrayRefLine, NULL);
        // early return, no need for explicit address-of TAC
        free_tac(addrOfLine);
        addrOfLine = NULL;
        arrayRefLine->operands.arrayLoad.array.name.variable->mustSpill = true;
        return &arrayRefLine->operands.arrayLoad.destination;
    }
    break;

    case T_DOT:
    {
        // walk_field_access can do everything we need
        // the only thing we have to do is ensure we have an LEA at the end instead of a direct read in the case of the dot operator
        struct TACLine *fieldAccessLine = walk_field_access(tree->child, block, scope, tacIndex, tempNum, &addrOfLine->operands.addrof.source, 0);
        convert_field_load_to_lea(fieldAccessLine, &addrOfLine->operands.addrof.source);
        // free the line created at the top of this function and return early
        free_tac(addrOfLine);

        fieldAccessLine->operands.fieldLoad.source.name.variable->mustSpill = true;
        return &fieldAccessLine->operands.fieldLoad.destination;
    }
    break;

    default:
        log_tree(LOG_FATAL, tree, "Address of operator is not supported for non-identifiers! Saw %s", token_get_name(tree->child->type));
    }

    addrOfLine->operands.addrof.source.name.variable->mustSpill = 1;

    struct Type typeOfAddress = *tac_operand_get_type(&addrOfLine->operands.addrof.source);
    typeOfAddress.pointerLevel++;
    tac_operand_populate_as_temp(scope, &addrOfLine->operands.addrof.destination, tempNum, &typeOfAddress);
    basic_block_append(block, addrOfLine, tacIndex);

    return &addrOfLine->operands.addrof.destination;
}

void walk_pointer_arithmetic(struct Ast *tree,
                             struct BasicBlock *block,
                             struct Scope *scope,
                             size_t *tacIndex,
                             size_t *tempNum,
                             struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_pointer_arithmetic");

    if ((tree->type != T_ADD) && (tree->type != T_SUBTRACT))
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_pointer_arithmetic!", token_get_name(tree->type));
    }

    struct Ast *pointerArithLhs = tree->child;
    struct Ast *pointerArithRhs = tree->child->sibling;

    struct TACLine *pointerArithmetic = new_tac_line(TT_ADD, tree->child);
    if (tree->type == T_SUBTRACT)
    {
        pointerArithmetic->operation = TT_SUBTRACT;
    }

    walk_sub_expression(pointerArithLhs, block, scope, tacIndex, tempNum, &pointerArithmetic->operands.arithmetic.sourceA);

    struct Type *pointerArithLhsType = tac_operand_get_type(&pointerArithmetic->operands.arithmetic.sourceA);
    tac_operand_populate_as_temp(scope, &pointerArithmetic->operands.arithmetic.destination, tempNum, pointerArithLhsType);

    struct TACLine *scaleMultiplication = set_up_scale_multiplication(pointerArithRhs,
                                                                      scope,
                                                                      tacIndex,
                                                                      tempNum,
                                                                      pointerArithLhsType,
                                                                      pointerArithLhsType);
    walk_sub_expression(pointerArithRhs, block, scope, tacIndex, tempNum, &scaleMultiplication->operands.arithmetic.sourceA);
    tac_operand_populate_as_temp(scope, &pointerArithmetic->operands.arithmetic.destination, tempNum, pointerArithLhsType);

    pointerArithmetic->operands.arithmetic.sourceB = scaleMultiplication->operands.arithmetic.destination;

    basic_block_append(block, scaleMultiplication, tacIndex);
    basic_block_append(block, pointerArithmetic, tacIndex);

    *destinationOperand = pointerArithmetic->operands.arithmetic.destination;
}

void walk_asm_block(struct Ast *tree,
                    struct BasicBlock *block,
                    struct Scope *scope,
                    size_t *tacIndex,
                    size_t *tempNum)
{
    log_tree(LOG_DEBUG, tree, "walk_asm_block");

    if (tree->type != T_ASM)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_asm_block!", token_get_name(tree->type));
    }

    struct Ast *asmRunner = tree->child;
    while (asmRunner != NULL)
    {
        struct TACLine *asmLine = NULL;
        switch (asmRunner->type)
        {
        case T_ASM:
        {
            asmLine = new_tac_line(TT_ASM, asmRunner);
            asmLine->operands.asm_.asmString = asmRunner->value;
        }
        break;

        case T_ASM_READVAR:
        {
            asmLine = new_tac_line(TT_ASM_LOAD, asmRunner);
            walk_sub_expression(asmRunner->child->sibling, block, scope, tacIndex, tempNum, &asmLine->operands.asmLoad.sourceOperand);
            asmLine->operands.asmLoad.destRegisterName = asmRunner->child->value;
        }
        break;

        case T_ASM_WRITEVAR:
        {
            asmLine = new_tac_line(TT_ASM_STORE, asmRunner);
            tac_operand_populate_from_variable(&asmLine->operands.asmStore.destinationOperand, scope_lookup_var(scope, asmRunner->child));
            asmLine->operands.asmStore.sourceRegisterName = asmRunner->child->sibling->value;
        }
        break;

        default:
            log_tree(LOG_FATAL, tree, "Non-asm seen as contents of ASM block!");
        }
        basic_block_append(block, asmLine, tacIndex);

        asmRunner = asmRunner->sibling;
    }
}

void walk_string_literal(struct Ast *tree,
                         struct BasicBlock *block,
                         struct Scope *scope,
                         struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_string_literal");

    if (tree->type != T_STRING_LITERAL)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_string_literal!", token_get_name(tree->type));
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
    struct ScopeMember *existingMember = scope_lookup(scope, stringName, E_VARIABLE);

    // if we already have a string literal for this thing, nothing else to do
    if (existingMember == NULL)
    {
        struct Ast fakeStringTree;
        fakeStringTree.value = stringName;
        fakeStringTree.sourceFile = tree->sourceFile;
        fakeStringTree.sourceLine = tree->sourceLine;
        fakeStringTree.sourceCol = tree->sourceCol;

        struct Type stringType;
        type_set_basic_type(&stringType, VT_ARRAY, NULL, 0);
        struct Type charType;
        type_init(&charType);
        charType.basicType = VT_U8;
        stringType.array.type = type_duplicate(&charType);
        stringType.array.size = stringLength;

        stringLiteralEntry = scope_create_variable(scope, &fakeStringTree, &stringType, true, A_PUBLIC);
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
    tac_operand_populate_from_variable(destinationOperand, stringLiteralEntry);
}

void walk_sizeof(struct Ast *tree,
                 struct BasicBlock *block,
                 struct Scope *scope,
                 size_t *tacIndex,
                 size_t *tempNum,
                 struct TACOperand *destinationOperand)
{
    log_tree(LOG_DEBUG, tree, "walk_sizeof");

    if (tree->type != T_SIZEOF)
    {
        log_tree(LOG_FATAL, tree, "Wrong AST (%s) passed to walk_sizeof!", token_get_name(tree->type));
    }

    struct TACLine *sizeofLine = new_tac_line(TT_SIZEOF, tree);
    struct TacSizeof *operands = &sizeofLine->operands.sizeof_;
    struct Type sizeType = {0};
    type_set_basic_type(&sizeType, VT_U64, NULL, 0);
    tac_operand_populate_as_temp(scope, &operands->destination, tempNum, &sizeType);

    switch (tree->child->type)
    {
    // if we see an identifier, it may be an identifier or a struct name
    case T_IDENTIFIER:
    {
        // do a generic scope lookup on the identifier
        struct VariableEntry *lookedUpVariable = scope_lookup_var_by_string(scope, tree->child->value);
        if (lookedUpVariable != NULL)
        {
            operands->type = type_duplicate_non_pointer(&lookedUpVariable->type);
        }
        else
        {
            struct Type identifierType = walk_non_pointer_type_name(scope, tree->child, scope->parentFunction->implementedFor);
            operands->type = identifierType;
        }
    }
    break;

    case T_TYPE_NAME:
    {
        walk_type_name(tree->child, scope, &operands->type, NULL);
    }
    break;

    default:
        log_tree(LOG_FATAL, tree, "sizeof is only supported on type names and identifiers!");
    }

    basic_block_append(block, sizeofLine, tacIndex);
    *destinationOperand = operands->destination;
}
