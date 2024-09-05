#include "drop.h"

#include "linearizer_generic.h"
#include "log.h"
#include "regalloc_generic.h"
#include "substratum_defs.h"
#include "symtab.h"

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

    printf("%zu is latest in scope %s\n", latestBlockInScope->labelNum, scope->name);

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
        (void)drop;
        // struct Ast dummyDropTree = {0};
        // dummyDropTree.sourceFile = "intrinsic";
        // struct TACLine *dropLine = new_tac_line(TT_METHOD_CALL, &dummyDropTree);
        // dropLine->operands.methodCall.methodName = "drop";
        // dropLine->operands.methodCall.arguments = deque_new(NULL);
        // struct TACOperand *dropArg = malloc(sizeof(struct TACOperand));

        // tac_operand_populate_from_variable(dropArg, drop);
        // deque_push_back(dropLine->operands.methodCall.arguments, dropArg);

        // tac_operand_populate_from_variable(&dropLine->operands.methodCall.calledOn, drop);
        // basic_block_append(latestBlockInScope, dropLine, &maxIndex);
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

void add_drops(struct SymbolTable *table)
{
    add_drops_to_scope(table->globalScope, NULL);
}
