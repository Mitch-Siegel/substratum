#include "symtab_trait.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include <string.h>

struct TraitEntry *trait_new(char *name, struct Scope *parentScope)
{
    struct TraitEntry *newTrait = malloc(sizeof(struct TraitEntry));
    newTrait->name = name;
    newTrait->public = set_new((void (*)(void *))function_entry_free, function_entry_compare);
    newTrait->private = set_new((void (*)(void *))function_entry_free, function_entry_compare);
    return newTrait;
}

void trait_free(struct TraitEntry *trait)
{
    set_free(trait->public);
    set_free(trait->private);
    free(trait);
}

void trait_entry_print(struct TraitEntry *trait, size_t depth, FILE *outFile)
{
    Iterator *funIter = NULL;
    for (funIter = set_begin(trait->public); iterator_gettable(funIter); iterator_next(funIter))
    {
        struct FunctionEntry *function = iterator_get(funIter);
        for (size_t i = 0; i < depth; i++)
        {
            fprintf(outFile, "\t");
        }
        char *signature = sprint_function_signature(function);
        fprintf(outFile, "public %s\n", signature);
        free(signature);
    }
    iterator_free(funIter);
    for(funIter = set_begin(trait->private); iterator_gettable(funIter); iterator_next(funIter))
    {
        struct FunctionEntry *function = iterator_get(funIter);
        for (size_t i = 0; i < depth; i++)
        {
            fprintf(outFile, "\t");
        }
        char *signature = sprint_function_signature(function);
        fprintf(outFile, "%s\n", signature);
        free(signature);
    }
    iterator_free(funIter);
}

ssize_t trait_entry_compare(void *traitDataA, void *traitDataB)
{
    struct TraitEntry *traitA = traitDataA;
    struct TraitEntry *traitB = traitDataB;
    return strcmp(traitA->name, traitB->name);
}
