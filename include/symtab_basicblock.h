#ifndef BASICBLOCK_H
#define BASICBLOCK_H

#include "tac.h"
#include "util.h"

struct BasicBlock
{
    struct LinkedList *TACList;
    size_t labelNum;
};

struct BasicBlock *BasicBlock_new(size_t labelNum);

void BasicBlock_free(struct BasicBlock *block);

void BasicBlock_append(struct BasicBlock *block, struct TACLine *line);

void printBasicBlock(struct BasicBlock *block, size_t indentLevel);

#endif
