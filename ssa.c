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