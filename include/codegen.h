#ifndef CODEGEN_H
#define CODEGEN_H
#include <stdio.h>

#include "substratum_defs.h"

struct SymbolTable;
struct CodegenState;
struct RegallocMetadata;
struct FunctionEntry;
struct VariableEntry;
struct StructDesc;
struct BasicBlock;
struct Scope;
struct LinkedList;
struct CodegenState;
struct RegallocMetadata;
struct MachineInfo;
struct TypeEntry;

void generate_code_for_program(struct SymbolTable *table,
                               FILE *outFile,
                               struct MachineInfo *info,
                               void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                               void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                               void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *),
                               bool emitStart);

void generate_code_for_type(struct CodegenState *globalContext,
                            struct TypeEntry *theType,
                            struct MachineInfo *info,
                            void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                            void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                            void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *));

void generate_code_for_struct(struct CodegenState *globalContext, struct StructDesc *theStruct,
                              struct MachineInfo *info,
                              void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                              void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                              void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *));

void generate_code_for_global_variable(struct CodegenState *globalContext, struct Scope *globalScope, struct VariableEntry *variable);

void generate_code_for_global_block(struct CodegenState *globalContext, struct Scope *globalScope, struct BasicBlock *globalBlock);

void generate_code_for_function(FILE *outFile,
                                struct FunctionEntry *function,
                                struct MachineInfo *info,
                                char *methodOfStructName, // NULL if not a method, otherwise the name of the struct which this function is a method of
                                void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                                void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                                void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *));

#endif
