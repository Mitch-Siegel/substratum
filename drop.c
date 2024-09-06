#include "drop.h"

#include "linearizer_generic.h"
#include "log.h"
#include "regalloc_generic.h"
#include "substratum_defs.h"
#include "symtab.h"

struct FunctionEntry *drop_create_function_prototype(struct Scope *scope)
{
    struct Ast dropFunctionDummyTree = {0};
    dropFunctionDummyTree.type = T_IDENTIFIER;
    dropFunctionDummyTree.value = DROP_TRAIT_FUNCTION_NAME;
    dropFunctionDummyTree.sourceFile = "intrinsic";

    struct FunctionEntry *dropFunction = function_entry_new(scope, &dropFunctionDummyTree, NULL);
    dropFunction->isMethod = true;

    struct Type selfArgType = {0};
    type_set_basic_type(&selfArgType, VT_SELF, NULL, 1);

    struct VariableEntry *selfArgEntry = variable_entry_new("self", &selfArgType, false, true, A_PUBLIC);
    scope_insert(dropFunction->mainScope, "self", selfArgEntry, E_ARGUMENT, A_PUBLIC);
    deque_push_back(dropFunction->arguments, selfArgEntry);

    return dropFunction;
}

void add_drops_to_basic_block(struct BasicBlock *block, struct RegallocMetadata *regalloc)
{
    size_t minIndex = SIZE_MAX;
    size_t maxIndex = 0;
    Iterator *tacIter = NULL;
    for (tacIter = list_begin(block->TACList); iterator_gettable(tacIter); iterator_next(tacIter))
    {
        struct TACLine *tac = iterator_get(tacIter);
        minIndex = MIN(minIndex, tac->index);
        maxIndex = MAX(maxIndex, tac->index);
    }
    iterator_free(tacIter);
}

void add_drops_to_function(struct FunctionEntry *function);

ssize_t basic_block_compare(void *a, void *b)
{
    struct BasicBlock *blockA = (struct BasicBlock *)a;
    struct BasicBlock *blockB = (struct BasicBlock *)b;
    return blockA->labelNum - blockB->labelNum;
}

void add_drops_to_scope(struct Scope *scope, struct RegallocMetadata *regalloc)
{
    struct BasicBlock *latestBlockInScope = NULL;

    Iterator *memberIter = NULL;

    Deque *drops = deque_new(NULL);

    for (memberIter = set_begin(scope->entries); iterator_gettable(memberIter); iterator_next(memberIter))
    {
        struct ScopeMember *member = iterator_get(memberIter);
        switch (member->type)
        {
        case E_FUNCTION:
            add_drops_to_function(member->entry);
            break;

        case E_VARIABLE:
        case E_ARGUMENT:
        {
            struct VariableEntry *dropCandidate = member->entry;
            if ((dropCandidate->name[0] != '.') && (type_is_struct_object(&dropCandidate->type) || type_is_enum_object(&dropCandidate->type)))
            {
                deque_push_back(drops, dropCandidate);
            }
        }
        break;

        case E_BASICBLOCK:
        {
            if (latestBlockInScope == NULL)
            {
                latestBlockInScope = member->entry;
            }
        }
        case E_SCOPE:
        case E_TYPE:
        case E_TRAIT:
            break;
        }
    }
    iterator_free(memberIter);

    if (latestBlockInScope != NULL)
    {
        Set *visitedBlocks = set_new(NULL, basic_block_compare);
        while (latestBlockInScope->successors->size > 0)
        {
            Iterator *succIter = NULL;
            for (succIter = set_begin(latestBlockInScope->successors); iterator_gettable(succIter); iterator_next(succIter))
            {
                struct BasicBlock *succ = iterator_get(succIter);
                if (set_find(visitedBlocks, succ))
                {
                    continue;
                }
                latestBlockInScope = succ;
            }
            iterator_free(succIter);
        }

        set_free(visitedBlocks);
    }
    else
    {
        if (drops->size > 0)
        {
            InternalError("Drops in scope %s but no basic block", scope->name);
        }
        else
        {
            deque_free(drops);
            return;
        }
    }

    size_t maxIndex = 0;
    struct TACLine *blockExitJump = NULL;

    if (latestBlockInScope->TACList->size > 0)
    {
        struct TACLine *lastLine = list_back(latestBlockInScope->TACList);
        maxIndex = lastLine->index;
        if (lastLine->operation == TT_JMP)
        {
            blockExitJump = list_pop_back(latestBlockInScope->TACList);
        }
        else
        {
            log_tree(LOG_FATAL, &lastLine->correspondingTree, "Last line in block is not a jump");
        }
    }

    while (drops->size > 0)
    {
        struct VariableEntry *drop = deque_pop_front(drops);

        struct Ast dummyDropTree = {0};
        dummyDropTree.sourceFile = "intrinsic";
        struct TACLine *dropLine = new_tac_line(TT_METHOD_CALL, &dummyDropTree);
        dropLine->operands.methodCall.methodName = DROP_TRAIT_FUNCTION_NAME;
        dropLine->operands.methodCall.arguments = deque_new(NULL);
        struct TACOperand *dropArg = malloc(sizeof(struct TACOperand));

        tac_operand_populate_from_variable(dropArg, drop);
        *dropArg = *get_addr_of_operand(&dummyDropTree, latestBlockInScope, scope, &maxIndex, dropArg);
        deque_push_back(dropLine->operands.methodCall.arguments, dropArg);

        tac_operand_populate_from_variable(&dropLine->operands.methodCall.calledOn, drop);
        basic_block_append(latestBlockInScope, dropLine, &maxIndex);
    }

    if (blockExitJump != NULL)
    {
        basic_block_append(latestBlockInScope, blockExitJump, &maxIndex);
    }
    deque_free(drops);
}

void add_drops_to_function(struct FunctionEntry *function)
{
    add_drops_to_scope(function->mainScope, &function->regalloc);
}

void implement_default_drop_for_struct(struct StructDesc *theStruct, struct FunctionEntry *dropFunction)
{
    struct BasicBlock *dropBlock = basic_block_new(0);
    scope_add_basic_block(dropFunction->mainScope, dropBlock);
    size_t dropTacIndex = 0;

    for (size_t fieldIdx = 0; fieldIdx < theStruct->fieldLocations->size; fieldIdx++)
    {
        struct StructField *field = deque_at(theStruct->fieldLocations, fieldIdx);
        // if a given field is a struct/enum type, work must be done to drop it as well
        if (type_is_struct_object(&field->variable->type) || type_is_enum_object(&field->variable->type))
        {
            struct Ast dummyDropTree = {0};
            dummyDropTree.sourceFile = "intrinsic";

            dropFunction->callsOtherFunction = true;

            struct TACLine *subDropLine = new_tac_line(TT_METHOD_CALL, &dummyDropTree);
            subDropLine->operands.methodCall.methodName = DROP_TRAIT_FUNCTION_NAME;
            subDropLine->operands.methodCall.arguments = deque_new(NULL);
            struct TACOperand *dropArg = malloc(sizeof(struct TACOperand));

            struct TACLine *fieldLeaLine = new_tac_line(TT_FIELD_LEA, &dummyDropTree);
            struct Type pointerType = type_duplicate_non_pointer(&field->variable->type);
            pointerType.pointerLevel++;
            tac_operand_populate_as_temp(dropFunction->mainScope, &fieldLeaLine->operands.fieldLoad.destination, &pointerType);
            tac_operand_populate_from_variable(&fieldLeaLine->operands.fieldLoad.source, scope_lookup_var_by_string(dropFunction->mainScope, "self"));
            fieldLeaLine->operands.fieldLoad.fieldName = field->variable->name;
            basic_block_append(dropBlock, fieldLeaLine, &dropTacIndex);

            *dropArg = fieldLeaLine->operands.fieldLoad.destination;
            deque_push_back(subDropLine->operands.methodCall.arguments, dropArg);

            tac_operand_populate_from_variable(&subDropLine->operands.methodCall.calledOn, field->variable);
            basic_block_append(dropBlock, subDropLine, &dropTacIndex);
        }
    }

    print_basic_block(dropBlock, 0);
    dropFunction->isDefined = true;
}

void implement_default_drop_for_enum(struct EnumDesc *theEnum, struct FunctionEntry *dropFunction)
{
}

void implement_default_drop_for_type(struct TypeEntry *type, struct Scope *scope)
{
    // if the type already implements Drop, nothing to do
    struct TraitEntry *dropTrait = type_entry_lookup_trait(type, DROP_TRAIT_NAME);
    if (dropTrait != NULL)
    {
        return;
    }

    struct Ast dummyDropTraitTree = {0};
    dummyDropTraitTree.sourceFile = "intrinsic";
    dummyDropTraitTree.value = DROP_TRAIT_NAME;
    dropTrait = scope_lookup_trait(scope, &dummyDropTraitTree);

    struct FunctionEntry *dropFunction = drop_create_function_prototype(type->implemented);
    scope_insert(type->implemented, DROP_TRAIT_FUNCTION_NAME, dropFunction, E_FUNCTION, A_PRIVATE);
    dropFunction->implementedFor = type;

    switch (type->permutation)
    {
    case TP_PRIMITIVE:
        break;

    case TP_STRUCT:
        implement_default_drop_for_struct(type->data.asStruct, dropFunction);
        break;

    case TP_ENUM:
        implement_default_drop_for_enum(type->data.asEnum, dropFunction);
        break;
    }

    type_entry_add_implemented(type, dropFunction, A_PRIVATE);

    Set *implementedPrivate = set_new(NULL, function_entry_compare);
    Set *implementedPublic = set_new(NULL, function_entry_compare);
    set_insert(implementedPrivate, dropFunction);

    function_entry_print(dropFunction, true, 0, stderr);

    type_entry_verify_trait(&dummyDropTraitTree, type, dropTrait, implementedPrivate, implementedPublic);

    type_entry_resolve_capital_self(type);
}

void implement_default_drops_for_scope(struct Scope *scope)
{
    // iterate all scope members, recursing. If a member is a type, make sure it has an implementaiton of Drop
    Iterator *memberIter = NULL;
    for (memberIter = set_begin(scope->entries); iterator_gettable(memberIter); iterator_next(memberIter))
    {
        struct ScopeMember *member = iterator_get(memberIter);
        switch (member->type)
        {
        case E_FUNCTION:
        {
            struct FunctionEntry *function = member->entry;
            implement_default_drops_for_scope(function->mainScope);
        }
        break;

        case E_VARIABLE:
        case E_ARGUMENT:
        case E_BASICBLOCK:
            break;

        case E_SCOPE:
            implement_default_drops_for_scope(member->entry);
            break;

        case E_TYPE:
            implement_default_drop_for_type(member->entry, scope);
            break;

        case E_TRAIT:
            break;
        }
    }
    iterator_free(memberIter);
}

void add_drops(struct SymbolTable *table)
{
    implement_default_drops_for_scope(table->globalScope);
    add_drops_to_scope(table->globalScope, NULL);
}
