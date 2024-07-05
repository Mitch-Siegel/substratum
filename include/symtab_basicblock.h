#ifndef BASICBLOCK_H
#define BASICBLOCK_H

#include "tac.h"
#include "util.h"

struct BasicBlock
{
    struct LinkedList *TACList;
    ssize_t labelNum; // ssize_t because some linearization functions take a label number < 0 as a signal to structure control flow
};

struct BasicBlock *basic_block_new(ssize_t labelNum);

void basic_block_free(struct BasicBlock *block);

void basic_block_append(struct BasicBlock *block, struct TACLine *line, size_t *tacIndex);

void basic_block_prepend(struct BasicBlock *block, struct TACLine *line);

void print_basic_block(struct BasicBlock *block, size_t indentLevel);

#endif
