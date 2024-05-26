#ifndef CODEGEN_H
#define CODEGEN_H
#include <stdio.h>

#include "substratum_defs.h"

struct SymbolTable;
struct CodegenState;
struct CodegenMetadata;
struct FunctionEntry;
struct VariableEntry;
struct StructEntry;
struct BasicBlock;
struct Scope;
struct LinkedList;
struct CodegenState;
struct CodegenMetadata;
struct MachineInfo;

void generateCodeForProgram(struct SymbolTable *table,
                            FILE *outFile,
                            void (*emitPrologue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *),
                            void (*emitEpilogue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, char *),
                            void (*generateCodeForBasicBlock)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, struct BasicBlock *, char *));

void generateCodeForStruct(struct CodegenState *globalContext, struct StructEntry *theStruct,
                           void (*emitPrologue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *),
                           void (*emitEpilogue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, char *),
                           void (*generateCodeForBasicBlock)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, struct BasicBlock *, char *));

void generateCodeForGlobalVariable(struct CodegenState *globalContext, struct Scope *globalScope, struct VariableEntry *variable);

void generateCodeForGlobalBlock(struct CodegenState *globalContext, struct Scope *globalScope, struct BasicBlock *globalBlock);

void generateCodeForFunction(FILE *outFile,
                             struct FunctionEntry *function,
                             char *methodOfStructName, // NULL if not a method, otherwise the name of the struct which this function is a method of
                             void (*emitPrologue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *),
                             void (*emitEpilogue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, char *),
                             void (*generateCodeForBasicBlock)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, struct BasicBlock *, char *));

void generateCodeForBasicBlock(struct CodegenState *context,
                               struct CodegenMetadata *metadata,
                               struct BasicBlock *block,
                               struct Scope *scope,
                               struct LinkedList *lifetimes,
                               char *functionName);

#endif
