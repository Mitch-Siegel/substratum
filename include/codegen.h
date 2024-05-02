#ifndef CODEGEN_H
#define CODEGEN_H
#include <stdio.h>

#include "substratum_defs.h"

struct SymbolTable;
struct CodegenContext;
struct CodegenMetadata;
struct FunctionEntry;
struct VariableEntry;
struct ClassEntry;
struct BasicBlock;
struct Scope;
struct LinkedList;

void generateCodeForProgram(struct SymbolTable *table, FILE *outFile);

void generateCodeForClass(struct CodegenContext *globalContext, struct ClassEntry *class);

void generateCodeForGlobalVariable(struct CodegenContext *globalContext, struct Scope *globalScope, struct VariableEntry *variable);

void generateCodeForGlobalBlock(struct CodegenContext *globalContext, struct Scope *globalScope, struct BasicBlock *globalBlock);

void emitPrologue(struct CodegenContext *context, struct CodegenMetadata *metadata);

void emitEpilogue(struct CodegenContext *context, struct CodegenMetadata *metadata);

void generateCodeForFunction(FILE *outFile,
                             struct FunctionEntry *function,
                             char *methodOfClassName); // NULL if not a method, otherwise the name of the class which this function is a method of

void generateCodeForBasicBlock(struct CodegenContext *context,
                               struct BasicBlock *block,
                               struct Scope *scope,
                               struct LinkedList *lifetimes,
                               char *functionName,
                               u8 reservedRegisters[3]);

#endif
