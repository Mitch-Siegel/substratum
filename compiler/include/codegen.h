#include "codegen_generic.h"
#include "symtab.h"
#include "util.h"

#ifndef _CODEGEN_OPT0_H_
#define _CODEGEN_OPT0_H_

void generateCodeForProgram(struct SymbolTable *table, FILE *outFile);

void generateCodeForFunction(FILE *outFile, struct FunctionEntry *function);

void generateCodeForBasicBlock(struct CodegenContext *context,
                               struct BasicBlock *block,
                               struct Scope *scope,
                               struct LinkedList *lifetimes,
                               char *functionName,
                               int reservedRegisters[3]);

#endif
