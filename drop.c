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
    log(LOG_DEBUG, "Adding drops to scope %s", scope->name);

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

    // don't add drops to the global scope - realistically this is a leak for anything that happens at the global scope
    // however, a true .ctors/.dtors implementation would be required to fix all the issues with globals and that's not on the table to get drops working.
    if ((scope->parentScope != NULL) && (strcmp(scope->name, "Global")))
    {

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
    }

    deque_free(drops);
}

void add_drops_to_function(struct FunctionEntry *function)
{
    if (!function->isDefined)
    {
        return;
    }
    log(LOG_DEBUG, "Adding drops to function %s", function->name);
    add_drops_to_scope(function->mainScope, &function->regalloc);
}

struct TACLine *generate_subdrop_tac(struct TACOperand *dropArg, struct Type *droppedType)
{
    struct Ast dummyDropTree = {0};
    dummyDropTree.sourceFile = "intrinsic";

    struct TACLine *subDropLine = new_tac_line(TT_METHOD_CALL, &dummyDropTree);
    subDropLine->operands.methodCall.methodName = DROP_TRAIT_FUNCTION_NAME;
    subDropLine->operands.methodCall.arguments = deque_new(NULL);

    struct TACOperand *dropArgCopy = malloc(sizeof(struct TACOperand));
    *dropArgCopy = *dropArg;

    // cheesy sort of hack to get later calls to tac_operand_get_type() on this operand to return the correct type
    subDropLine->operands.methodCall.calledOn.permutation = VP_LITERAL_VAL;
    subDropLine->operands.methodCall.calledOn.castAsType = *droppedType;

    deque_push_back(subDropLine->operands.methodCall.arguments, dropArgCopy);

    return subDropLine;
}

void implement_default_drop_for_struct(struct StructDesc *theStruct, struct FunctionEntry *dropFunction)
{
    struct BasicBlock *dropBlock = basic_block_new(FUNCTION_EXIT_BLOCK_LABEL + 1);
    scope_add_basic_block(dropFunction->mainScope, dropBlock);
    size_t dropTacIndex = 0;

    // iterate over all fields in the struct, and if a field is a struct/enum type, drop it
    for (size_t fieldIdx = 0; fieldIdx < theStruct->fieldLocations->size; fieldIdx++)
    {
        struct StructField *field = deque_at(theStruct->fieldLocations, fieldIdx);
        if (type_is_struct_object(&field->variable->type) || type_is_enum_object(&field->variable->type))
        {
            struct Ast dummyDropTree = {0};
            dummyDropTree.sourceFile = "intrinsic";

            dropFunction->callsOtherFunction = true;

            // create a drop call for the field

            // lea the field, pass as argument to subdrop
            struct TACLine *fieldLeaLine = new_tac_line(TT_FIELD_LEA, &dummyDropTree);
            struct Type pointerType = type_duplicate_non_pointer(&field->variable->type);
            pointerType.pointerLevel++;
            tac_operand_populate_as_temp(dropFunction->mainScope, &fieldLeaLine->operands.fieldLoad.destination, &pointerType);
            tac_operand_populate_from_variable(&fieldLeaLine->operands.fieldLoad.source, scope_lookup_var_by_string(dropFunction->mainScope, "self"));
            fieldLeaLine->operands.fieldLoad.fieldName = field->variable->name;
            basic_block_append(dropBlock, fieldLeaLine, &dropTacIndex);

            basic_block_append(dropBlock, generate_subdrop_tac(&fieldLeaLine->operands.fieldLoad.destination, &field->variable->type), &dropTacIndex);
        }
    }

    dropFunction->isDefined = true;
}

void enum_default_drop_add_match_and_drop_for_member(struct FunctionEntry *dropFunction,
                                                     struct TACOperand *matchedAgainstNumerical,
                                                     struct EnumMember *member,
                                                     struct BasicBlock *dropMatchBlock,
                                                     size_t *dropTacIndex,
                                                     struct BasicBlock *afterMatchBlock,
                                                     ssize_t *labelNum)
{
    struct Ast dummyDropTree = {0};
    dummyDropTree.sourceFile = "intrinsic";

    struct BasicBlock *dropCaseBlock = basic_block_new((*labelNum)++);
    scope_add_basic_block(dropFunction->mainScope, dropCaseBlock);

    // set up the beq to match this member
    struct TACLine *matchJump = new_tac_line(TT_BEQ, &dummyDropTree);

    matchJump->operands.conditionalBranch.sourceA.name.val = member->numerical;
    matchJump->operands.conditionalBranch.sourceA.castAsType = *tac_operand_get_type(matchedAgainstNumerical);
    matchJump->operands.conditionalBranch.sourceA.permutation = VP_LITERAL_VAL;

    matchJump->operands.conditionalBranch.sourceB = *matchedAgainstNumerical;
    matchJump->operands.conditionalBranch.sourceB.castAsType.basicType = VT_U64; // TODO: size_t definition?

    matchJump->operands.conditionalBranch.label = dropCaseBlock->labelNum;
    basic_block_append(dropMatchBlock, matchJump, dropTacIndex);

    size_t branchTacIndex = *dropTacIndex;

    struct TACLine *compAddrOfEnumData = new_tac_line(TT_ADD, &dummyDropTree);
    tac_operand_populate_from_variable(&compAddrOfEnumData->operands.arithmetic.sourceA, scope_lookup_var_by_string(dropFunction->mainScope, "self"));
    compAddrOfEnumData->operands.arithmetic.sourceB.name.val = sizeof(size_t);
    compAddrOfEnumData->operands.arithmetic.sourceB.permutation = VP_LITERAL_VAL;
    compAddrOfEnumData->operands.arithmetic.sourceB.castAsType.basicType = select_variable_type_for_number(sizeof(size_t));

    struct Type memberPointerType = type_duplicate_non_pointer(&member->type);
    memberPointerType.pointerLevel++;

    tac_operand_populate_as_temp(dropFunction->mainScope, &compAddrOfEnumData->operands.arithmetic.destination, &memberPointerType);
    struct TACOperand *enumDataPtr = &compAddrOfEnumData->operands.arithmetic.destination;
    basic_block_append(dropCaseBlock, compAddrOfEnumData, &branchTacIndex);

    basic_block_append(dropCaseBlock, generate_subdrop_tac(enumDataPtr, &member->type), &branchTacIndex);

    struct TACLine *jumpToAfterMatch = new_tac_line(TT_JMP, &dummyDropTree);
    jumpToAfterMatch->operands.jump.label = afterMatchBlock->labelNum;
    basic_block_append(dropCaseBlock, jumpToAfterMatch, &branchTacIndex);
}

void implement_default_drop_for_enum(struct EnumDesc *theEnum, struct FunctionEntry *dropFunction)
{
    struct Ast dummyDropTree = {0};
    dummyDropTree.sourceFile = "intrinsic";

    struct BasicBlock *dropAfterMatchBlock = basic_block_new(FUNCTION_EXIT_BLOCK_LABEL);

    ssize_t labelNum = FUNCTION_EXIT_BLOCK_LABEL + 1;
    struct BasicBlock *dropMatchBlock = basic_block_new(labelNum++);

    scope_add_basic_block(dropFunction->mainScope, dropMatchBlock);
    size_t dropTacIndex = 0;

    struct TACLine *loadMatchedAgainst = new_tac_line(TT_LOAD, &dummyDropTree);

    tac_operand_populate_from_variable(&loadMatchedAgainst->operands.load.address, scope_lookup_var_by_string(dropFunction->mainScope, "self"));

    struct Type numericalType = {0};
    type_set_basic_type(&numericalType, VT_U64, NULL, 0);
    tac_operand_populate_as_temp(dropFunction->mainScope, &loadMatchedAgainst->operands.load.destination, &numericalType);
    struct TACOperand *matchedAgainstNumerical = &loadMatchedAgainst->operands.load.destination;
    basic_block_append(dropMatchBlock, loadMatchedAgainst, &dropTacIndex);

    Iterator *memberIter = NULL;
    for (memberIter = set_begin(theEnum->members); iterator_gettable(memberIter); iterator_next(memberIter))
    {
        struct EnumMember *checkedMember = iterator_get(memberIter);
        if (type_is_struct_object(&checkedMember->type) || type_is_enum_object(&checkedMember->type))
        {
            enum_default_drop_add_match_and_drop_for_member(dropFunction,
                                                            matchedAgainstNumerical,
                                                            checkedMember,
                                                            dropMatchBlock,
                                                            &dropTacIndex,
                                                            dropAfterMatchBlock,
                                                            &labelNum);
        }
    }

    struct TACLine *nonMatchJump = new_tac_line(TT_JMP, &dummyDropTree);
    nonMatchJump->operands.jump.label = dropAfterMatchBlock->labelNum;
    basic_block_append(dropMatchBlock, nonMatchJump, &dropTacIndex);

    iterator_free(memberIter);

    scope_add_basic_block(dropFunction->mainScope, dropAfterMatchBlock);

    dropFunction->isDefined = true;
}

void implement_default_drop_for_non_generic_type(struct TypeEntry *type, struct Scope *scope)
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

    switch (type->genericType)
    {
    case G_NONE:
    case G_INSTANCE:
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
        break;

    case G_BASE:
        InternalError("Generic base type %s seen in implement_default_drop_for_non_generic_type", type_entry_name(type));
        break;
    }

    type_entry_add_implemented(type, dropFunction, A_PRIVATE);

    Set *implementedPrivate = set_new(NULL, function_entry_compare);
    Set *implementedPublic = set_new(NULL, function_entry_compare);
    set_insert(implementedPrivate, dropFunction);

    type_entry_verify_trait(&dummyDropTraitTree, type, dropTrait, implementedPrivate, implementedPublic);

    switch (type->genericType)
    {
    case G_NONE:
        type_entry_resolve_capital_self(type);
        break;

    case G_BASE:
        InternalError("Generic base type %s seen in implement_default_drop_for_non_generic_type", type_entry_name(type));
        break;

    case G_INSTANCE:
        type_entry_resolve_capital_self(type);
        break;
    }
}

void implement_default_drop_for_type(struct TypeEntry *type, struct Scope *scope)
{
    switch (type->genericType)
    {
    case G_NONE:
    case G_INSTANCE:
        implement_default_drop_for_non_generic_type(type, scope);
        break;

    case G_BASE:
    {
        Iterator *instanceIter = NULL;
        for (instanceIter = hash_table_begin(type->generic.base.instances); iterator_gettable(instanceIter); iterator_next(instanceIter))
        {
            HashTableEntry *instanceEntry = iterator_get(instanceIter);
            implement_default_drop_for_non_generic_type(instanceEntry->value, scope);
        }
        iterator_free(instanceIter);
    }
    break;
    }
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
