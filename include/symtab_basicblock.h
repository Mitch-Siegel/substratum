#ifndef BASICBLOCK_H
#define BASICBLOCK_H

#include "util.h"
#include "tac.h"

struct BasicBlock
{
	struct LinkedList *TACList;
	int labelNum;
};

struct BasicBlock *BasicBlock_new(int labelNum);

void BasicBlock_free(struct BasicBlock *b);

void BasicBlock_append(struct BasicBlock *b, struct TACLine *l);

void printBasicBlock(struct BasicBlock *b, size_t indentLevel);

#endif
