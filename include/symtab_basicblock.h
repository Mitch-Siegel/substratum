#ifndef BASICBLOCK_H
#define BASICBLOCK_H

#include "tac.h"
#include "util.h"

struct BasicBlock
{
    struct LinkedList *TACList;
    int labelNum;
};

struct BasicBlock *BasicBlock_new(int labelNum);

void BasicBlock_free(struct BasicBlock *block);

void BasicBlock_append(struct BasicBlock *block, struct TACLine *line);

void printBasicBlock(struct BasicBlock *block, size_t indentLevel);

#endif
