#ifndef BASICBLOCK_H
#define BASICBLOCK_H

#include "tac.h"
#include "util.h"

#include "mbcl/list.h"
#include "mbcl/set.h"

#define FUNCTION_EXIT_BLOCK_LABEL ((ssize_t)0)

struct StructDesc;

struct BasicBlock
{
    List *TACList;
    Set *successors;  // set of ssize_t labels of blocks with control flow pointing from this block
    ssize_t labelNum; // ssize_t because some linearization functions take a label number < 0 as a signal to structure control flow
};

struct BasicBlock *basic_block_new(ssize_t labelNum);

void basic_block_add_successor(struct BasicBlock *block, ssize_t successor);

void basic_block_free(struct BasicBlock *block);

void basic_block_append(struct BasicBlock *block, struct TACLine *line, size_t *tacIndex);

void basic_block_prepend(struct BasicBlock *block, struct TACLine *line);

void print_basic_block(struct BasicBlock *block, size_t indentLevel);

void basic_block_resolve_capital_self(struct BasicBlock *block, struct TypeEntry *typeEntry);

#endif
