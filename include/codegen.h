#ifndef CODEGEN_H
#define CODEGEN_H
#include <stdio.h>

#include "substratum_defs.h"

struct SymbolTable;
struct CodegenState;
struct RegallocMetadata;
struct FunctionEntry;
struct VariableEntry;
struct StructEntry;
struct BasicBlock;
struct Scope;
struct LinkedList;
struct CodegenState;
struct RegallocMetadata;
struct MachineInfo;

void generateCodeForProgram(struct SymbolTable *table,
                            FILE *outFile,
                            struct MachineInfo *info,
                            void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                            void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                            void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *));

void generateCodeForStruct(struct CodegenState *globalContext, struct StructEntry *theStruct,
                           struct MachineInfo *info,
                           void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                           void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                           void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *));

void generateCodeForGlobalVariable(struct CodegenState *globalContext, struct Scope *globalScope, struct VariableEntry *variable);

void generateCodeForGlobalBlock(struct CodegenState *globalContext, struct Scope *globalScope, struct BasicBlock *globalBlock);

void generateCodeForFunction(FILE *outFile,
                             struct FunctionEntry *function,
                             struct MachineInfo *info,
                             char *methodOfStructName, // NULL if not a method, otherwise the name of the struct which this function is a method of
                             void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                             void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                             void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *));

#endif
