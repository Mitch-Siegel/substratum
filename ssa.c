#include "ssa.h"
#include "symtab.h"

int compareBlockNumbers(void *numberA, void *numberB)
{
    return (ssize_t)numberA != (ssize_t)numberB;
}

int compareTacOperand(void *dataA, void *dataB)
{
    struct TACOperand *operandA = dataA;
    struct TACOperand *operandB = dataB;

    if (Type_Compare(&operandA->type, &operandB->type))
    {
        return 1;
    }

    if (Type_Compare(&operandA->castAsType, &operandB->castAsType))
    {
        return 1;
    }

    if ((operandA->permutation != vp_literal && (operandB->permutation != vp_literal)))
    {
        if (strcmp(operandA->name.str, operandB->name.str))
        {
            return 1;
        }
    }
    else if (operandA->permutation != operandB->permutation)
    {
        return 1;
    }

    if (operandA->ssaNumber != operandB->ssaNumber)
    {
        return 1;
    }

    return 0;
}

void printDataFlwosAsDot(struct LinkedList **blockFlows, struct FunctionEntry *function)
{
    printf("digraph%s{\nedge[dir=forward]\nnode[shape=plaintext,style=filled]\n", function->name);
    for (size_t blockIndex = 0; blockIndex < function->BasicBlockList->size; blockIndex++)
    {
        for (struct LinkedListNode *flowRunner = blockFlows[blockIndex]->head; flowRunner != NULL; flowRunner = flowRunner->next)
        {
            printf("%s_%zu:s->%s_%zu:n\n", function->name, blockIndex, function->name, (size_t)flowRunner->data);
        }
    }

    for (struct LinkedListNode *blockRunner = function->BasicBlockList->head; blockRunner != NULL; blockRunner = blockRunner->next)
    {
        struct BasicBlock *thisBlock = blockRunner->data;
        printf("%s_%zu[label=<%s_%zu<BR />\n", function->name, thisBlock->labelNum, function->name, thisBlock->labelNum);

        for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            char *tacString = sPrintTACLine(tacRunner->data);
            printf("%s<BR />\n", tacString);
            free(tacString);
        }
        printf(">]\n");
    }

    printf("}\n\n\n");
}

struct LinkedList **generateBlockFlows(struct LinkedList *blocks)
{
    struct LinkedList **blockFlows = malloc(blocks->size * sizeof(struct LinkedList *));

    for (size_t blockIndex = 0; blockIndex < blocks->size; blockIndex++)
    {
        blockFlows[blockIndex] = LinkedList_New();
    }

    for (struct LinkedListNode *blockRunner = blocks->head; blockRunner != NULL; blockRunner = blockRunner->next)
    {
        struct BasicBlock *thisBlock = blockRunner->data;
        struct LinkedList *thisBlockFlows = blockFlows[thisBlock->labelNum];

        for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            struct TACLine *thisTAC = tacRunner->data;
            switch (thisTAC->operation)
            {
            case tt_beq:
            case tt_bne:
            case tt_bgeu:
            case tt_bltu:
            case tt_bgtu:
            case tt_bleu:
            case tt_beqz:
            case tt_bnez:
            case tt_jmp:
                if (LinkedList_Find(thisBlockFlows, compareBlockNumbers, (void *)thisTAC->operands[0].name.val) == NULL)
                {
                    LinkedList_Append(thisBlockFlows, (void *)thisTAC->operands[0].name.val);
                }
                break;

            default:
                break;
            }
        }
    }

    return blockFlows;
}

void recordLiveGen(struct LinkedList *blockGen, struct TACOperand *operand)
{
    if ((operand->permutation != vp_literal) && (LinkedList_Find(blockGen, compareTacOperand, operand) == NULL))
    {
        LinkedList_Append(blockGen, operand);
    }
}

struct LinkedList **computeLiveGens(struct LinkedList *blocks)
{
    struct LinkedList **blockGens = malloc(blocks->size * sizeof(struct LinkedList *));
    for (size_t blockIndex = 0; blockIndex < blocks->size; blockIndex++)
    {
        blockGens[blockIndex] = LinkedList_New();
    }

    for (struct LinkedListNode *blockRunner = blocks->head; blockRunner != NULL; blockRunner = blockRunner->next)
    {
        struct BasicBlock *thisBlock = blockRunner->data;
        struct LinkedList *thisBlockGens = blockGens[thisBlock->labelNum];

        for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            struct TACLine *thisTAC = tacRunner->data;
            switch (thisTAC->operation)
            {
            case tt_call:
                if (TAC_GetTypeOfOperand(thisTAC, 0)->basicType != vt_null)
                {
                    recordLiveGen(thisBlockGens, &thisTAC->operands[0]);
                }
                break;
            case tt_assign:
            case tt_add:
            case tt_subtract:
            case tt_mul:
            case tt_div:
            case tt_modulo:
            case tt_bitwise_and:
            case tt_bitwise_or:
            case tt_bitwise_not:
            case tt_bitwise_xor:
            case tt_lshift:
            case tt_rshift:
            case tt_load:
            case tt_load_off:
            case tt_load_arr:
            case tt_addrof:
            case tt_lea_off:
            case tt_lea_arr:
                recordLiveGen(thisBlockGens, &thisTAC->operands[0]);
                break;

            case tt_store:
            case tt_store_off:
            case tt_store_arr:
            case tt_beq:
            case tt_bne:
            case tt_bgeu:
            case tt_bltu:
            case tt_bgtu:
            case tt_bleu:
            case tt_beqz:
            case tt_bnez:
            case tt_jmp:
            case tt_stack_reserve:
            case tt_stack_store:
            case tt_label:
            case tt_return:
            case tt_do:
            case tt_enddo:
            case tt_asm:
                break;
            }
        }
    }

    return blockGens;
}

void generateSsaForFunction(struct FunctionEntry *function)
{
    printf("Generate ssa for %s\n", function->name);
    struct LinkedList **blockFlows = generateBlockFlows(function->BasicBlockList);
    printf("blockFlows: %p\n", blockFlows);

    struct LinkedList **blockGens = computeLiveGens(function->BasicBlockList);

    for (size_t blockIndex = 0; blockIndex < function->BasicBlockList->size; blockIndex++)
    {
        printf("%s_%zu\n", function->name, blockIndex);
        for (struct LinkedListNode *genRunner = blockGens[blockIndex]->head; genRunner != NULL; genRunner = genRunner->next)
        {
            struct TACOperand *thisGen = genRunner->data;
            printf("\tgen: %s\n", thisGen->name.str);
        }
    }
}

void generateSsa(struct SymbolTable *theTable)
{
    printf("generate ssa for %s\n", theTable->name);

    for (size_t entryIndex = 0; entryIndex < theTable->globalScope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = theTable->globalScope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case e_function:
            generateSsaForFunction(thisMember->entry);
            break;

        default:
            break;
        }
    }
}