#ifndef CODEGEN_H
#define CODEGEN_H
#include <stdio.h>

#include "substratum_defs.h"

struct SymbolTable;
struct CodegenContext;
struct CodegenMetadata;
struct FunctionEntry;
struct BasicBlock;
struct Scope;
struct LinkedList;

void generateCodeForProgram(struct SymbolTable *table, FILE *outFile);

void emitPrologue(struct CodegenContext *context, struct CodegenMetadata *metadata);

void emitEpilogue(struct CodegenContext *context, struct CodegenMetadata *metadata);

void generateCodeForFunction(FILE *outFile, struct FunctionEntry *function);

void generateCodeForBasicBlock(struct CodegenContext *context,
                               struct BasicBlock *block,
                               struct Scope *scope,
                               struct LinkedList *lifetimes,
                               char *functionName,
                               u8 reservedRegisters[3]);

#endif
