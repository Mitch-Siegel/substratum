#include "codegen.h"

#ifndef _CODEGEN_OPT0_H_
#define _CODEGEN_OPT0_H_

void generateCodeForProgram_0(struct SymbolTable *table, FILE *outFile, int regAllocOpt);

void generateCodeForFunction_0(FILE *outFile, struct FunctionEntry *function, int regAllocOpt);

void generateCodeForBasicBlock_0(FILE *outFile,
                               struct BasicBlock *block,
                               struct Scope *scope,
                               struct LinkedList *lifetimes,
                               char *functionName,
                               int reservedRegisters[3]);

#endif
