#include "ssa.h"
#include "symtab.h"

#include "livevars.h"

int compareBlockNumbers(void *numberA, void *numberB)
{
    return (ssize_t)numberA != (ssize_t)numberB;
}

void printDataFlowsAsDot(struct LinkedList **blockFlows, struct FunctionEntry *function)
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

void generateSsaForFunction(struct FunctionEntry *function)
{
    printf("Generate ssa for %s\n", function->name);

    struct IdfaContext *context = IdfaContext_Create(function->BasicBlockList);

    struct Idfa *liveVars = analyzeLiveVars(context);

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