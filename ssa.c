#include "ssa.h"
#include "symtab.h"

#include "livevars.h"

int compareBlockNumbers(void *numberA, void *numberB)
{
    return (ssize_t)numberA != (ssize_t)numberB;
}

void printControlFlowsAsDot(struct Idfa *idfa, char *functionName)
{
    printf("digraph %s{\nedge[dir=forward]\nnode[shape=plaintext,style=filled]\n", functionName);
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        for (struct LinkedListNode *flowRunner = idfa->context->successors[blockIndex]->elements->head; flowRunner != NULL; flowRunner = flowRunner->next)
        {
            struct BasicBlock *destinationBlock = flowRunner->data;
            printf("%s_%zu:s->%s_%zu:n\n", functionName, blockIndex, functionName, destinationBlock->labelNum);
        }

        struct BasicBlock *thisBlock = idfa->context->blocks[blockIndex];
        printf("%s_%zu[label=<%s_%zu<BR />\n", functionName, thisBlock->labelNum, functionName, thisBlock->labelNum);

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

void insertPhiFunctions(struct Idfa *liveVars)
{
    struct AST fakePhiTree;
    memset(&fakePhiTree, 0, sizeof(struct AST));

    for (size_t blockIndex = 0; blockIndex < liveVars->context->nBlocks; blockIndex++)
    {
        struct BasicBlock *phiBlock = liveVars->context->blocks[blockIndex];
        size_t blockEntryTacIndex = 0;
        if (phiBlock->TACList->size > 0)
        {
            blockEntryTacIndex = ((struct TACLine *)phiBlock->TACList->head->data)->index;
        }
        struct Set *phiVars = Set_New(compareTacOperand);
        for (struct LinkedListNode *predecessorRunner = liveVars->context->predecessors[blockIndex]->elements->head; predecessorRunner != NULL; predecessorRunner = predecessorRunner->next)
        {
            struct BasicBlock *predecessorBlock = predecessorRunner->data;
            for (struct LinkedListNode *liveVarRunner = liveVars->facts.out[predecessorBlock->labelNum]->elements->head; liveVarRunner != NULL; liveVarRunner = liveVarRunner->next)
            {
                struct TACOperand *liveOut = liveVarRunner->data;
                Set_Insert(phiVars, liveOut);
            }
        }

        for (struct LinkedListNode *phiVarRunner = phiVars->elements->head; phiVarRunner != NULL; phiVarRunner = phiVarRunner->next)
        {
            struct TACOperand *phiVar = phiVarRunner->data;
            struct TACLine *newPhi = newTACLine(blockEntryTacIndex, tt_phi, &fakePhiTree);
            newPhi->operands[0] = *phiVar;
            newPhi->operands[1] = *phiVar;
            newPhi->operands[2] = *phiVar;
            BasicBlock_prepend(phiBlock, newPhi);
        }
    }
}

struct TACOperand *ssaOperandLookupOrInsert(struct Set *ssaOperands, struct TACOperand *originalOperand)
{
    struct TACOperand *foundOperand = Set_Find(ssaOperands, originalOperand);
    if (foundOperand == NULL)
    {
        struct TACOperand *newOperand = malloc(sizeof(struct TACOperand));
        memcpy(newOperand, originalOperand, sizeof(struct TACOperand));
        newOperand->ssaNumber = 0;
        Set_Insert(ssaOperands, newOperand);
        return newOperand;
    }
    return foundOperand;
}

void renameWrittenTACOperands(struct IdfaContext *context)
{
    struct Set *ssaOperandNumbers = Set_New(compareTacOperandIgnoreSsaNumber);
    for (size_t blockIndex = 0; blockIndex < context->nBlocks; blockIndex++)
    {
        struct BasicBlock *thisBlock = context->blocks[blockIndex];
        for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            struct TACLine *thisTac = tacRunner->data;

            for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
            {
                if (getUseOfOperand(thisTac, operandIndex) == u_write)
                {
                    struct TACOperand *originalOperand = &thisTac->operands[operandIndex];
                    struct TACOperand *ssaOperand = ssaOperandLookupOrInsert(ssaOperandNumbers, originalOperand);
                    originalOperand->ssaNumber = ssaOperand->ssaNumber++;
                }
            }
        }
    }
}

size_t hashTacOperand(void *operand)
{
    size_t hash = 0;
    for (size_t byteIndex = 0; byteIndex < sizeof(struct TACOperand); byteIndex++)
    {
        hash += ((u8 *)operand)[byteIndex];
        hash <<= 1;
    }
    return hash;
}

struct Set *findHighestSsaLiveIns(struct Idfa *liveVars, size_t blockIndex)
{
    struct Set *highestSsaLiveIns = Set_New(compareTacOperandIgnoreSsaNumber);

    for (struct LinkedListNode *liveInRunner = liveVars->facts.in[blockIndex]->elements->head; liveInRunner != NULL; liveInRunner = liveInRunner->next)
    {
        struct TACOperand *thisLiveIn = liveInRunner->data;
        struct TACOperand *highestLiveIn = Set_Find(highestSsaLiveIns, thisLiveIn);
        if (highestLiveIn == NULL)
        {
            Set_Insert(highestSsaLiveIns, thisLiveIn);
        }
        else if (highestLiveIn->ssaNumber < thisLiveIn->ssaNumber)
        {
            Set_Delete(highestSsaLiveIns, highestLiveIn);
            Set_Insert(highestSsaLiveIns, thisLiveIn);
        }
    }

    return highestSsaLiveIns;
}

struct TACOperand *lookupMostRecentSsaAssignment(struct Set *highestSsaLiveIns, struct Set *assignmentsThisBlock, struct TACOperand *originalOperand)
{
    struct TACOperand *mostRecentAssignment = NULL;
    struct TACOperand *assignmentInBlock = Set_Find(assignmentsThisBlock, originalOperand);
    if (assignmentInBlock == NULL)
    {
        mostRecentAssignment = Set_Find(highestSsaLiveIns, originalOperand);
    }
    else
    {
        mostRecentAssignment = assignmentInBlock;
    }
    return mostRecentAssignment;
}

void renameReadTACOperands(struct Idfa *liveVars)
{
    for (size_t blockIndex = 0; blockIndex < liveVars->context->nBlocks; blockIndex++)
    {
        // struct HashTable *defNumbersThisBlock = HashTable_New(100, hashTacOperand, compareTacOperandIgnoreSsaNumber, NULL, NULL);
        struct Set *highestSsaLiveIns = findHighestSsaLiveIns(liveVars, blockIndex);
        struct Set *ssaLivesFromThisBlock = Set_New(compareTacOperandIgnoreSsaNumber);

        printf("SSA liveins for %zu:", blockIndex);
        for (struct LinkedListNode *liveInRunner = highestSsaLiveIns->elements->head; liveInRunner != NULL; liveInRunner = liveInRunner->next)
        {
            struct TACOperand *liveIn = liveInRunner->data;
            printTACOperand(liveIn);
            printf(" ");
        }
        printf("\n");
        struct BasicBlock *thisBlock = liveVars->context->blocks[blockIndex];
        for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            struct TACLine *thisTac = tacRunner->data;

            for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
            {
                struct TACOperand *thisOperand = &thisTac->operands[operandIndex];
                switch (getUseOfOperand(thisTac, operandIndex))
                {
                case u_unused:
                    break;

                case u_read:
                    struct TACOperand *mostRecentAssignment = lookupMostRecentSsaAssignment(highestSsaLiveIns, ssaLivesFromThisBlock, thisOperand);
                    if (mostRecentAssignment != NULL)
                    {
                        thisOperand->ssaNumber = mostRecentAssignment->ssaNumber;
                    }
                    break;

                case u_write:
                    break;
                    // struct TACOperand *originalOperand = &thisTac->operands[operandIndex];
                    // struct TACOperand *ssaOperand = ssaOperandLookupOrInsert(context->ssaOperands, originalOperand);
                    // originalOperand->ssaNumber = ssaOperand->ssaNumber;
                }
            }
        }
    }
}

void generateSsaForFunction(struct FunctionEntry *function)
{
    printf("Generate ssa for %s\n", function->name);

    struct IdfaContext *context = IdfaContext_Create(function->BasicBlockList);

    struct Idfa *liveVars = analyzeLiveVars(context);

    printControlFlowsAsDot(liveVars, function->name);
    Idfa_printFacts(liveVars);

    for (size_t blockIndex = 0; blockIndex < function->BasicBlockList->size; blockIndex++)
    {
        printf("%s_%zu\n", function->name, blockIndex);
        printf("gen: %zu kill: %zu in: %zu out: %zu\n",
               liveVars->facts.gen[blockIndex]->elements->size,
               liveVars->facts.kill[blockIndex]->elements->size,
               liveVars->facts.in[blockIndex]->elements->size,
               liveVars->facts.out[blockIndex]->elements->size);
    }

    insertPhiFunctions(liveVars);

    printControlFlowsAsDot(liveVars, function->name);

    renameWrittenTACOperands(context);

    printControlFlowsAsDot(liveVars, function->name);

    Idfa_Free(liveVars);
    liveVars = analyzeLiveVars(context);

    Idfa_printFacts(liveVars);
    renameReadTACOperands(liveVars);
    printControlFlowsAsDot(liveVars, function->name);
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