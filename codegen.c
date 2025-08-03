#include "codegen.h"

#include "codegen_generic.h"
#include "log.h"
#include "regalloc.h"
#include "regalloc_riscv.h"
#include "symtab.h"

void generate_code_for_program(struct SymbolTable *table,
                               FILE *outFile,
                               struct MachineInfo *info,
                               void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                               void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                               void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *),
                               bool emitStart)
{
    struct CodegenState globalContext;
    size_t globalInstructionIndex = 0;
    globalContext.instructionIndex = &globalInstructionIndex;
    globalContext.outFile = outFile;

    // fprintf(outFile, "\t.text\n");
    Iterator *entryIterator = NULL;
    for (entryIterator = set_begin(table->globalScope->entries); iterator_gettable(entryIterator); iterator_next(entryIterator))
    {
        struct ScopeMember *thisMember = iterator_get(entryIterator);
        switch (thisMember->type)
        {
        case E_FUNCTION:
        {
            struct FunctionEntry *generatedFunction = thisMember->entry;
            if (!generatedFunction->isDefined)
            {
                break;
            }

            // TODO: don't provide _start ourselves, call exit() when done. crt0.s??
            if (emitStart && !strcmp(generatedFunction->name, "main"))
            {
                fprintf(outFile, ".align 2\n\t.globl _start\n_start:\n\tcall main\n\tpgm_done:\n\tli a0, 0\n\tcall exit\n");
            }

            generate_code_for_function(outFile, generatedFunction, info, NULL, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
            fprintf(outFile, "\t.size %s, .-%s\n", generatedFunction->name, generatedFunction->name);
        }
        break;

        case E_BASICBLOCK:
        {
            generate_code_for_global_block(&globalContext, table->globalScope, thisMember->entry);
        }
        break;

        case E_VARIABLE:
        {
            generate_code_for_global_variable(&globalContext, table->globalScope, thisMember->entry);
        }
        break;

        case E_TYPE:
        {
            generate_code_for_type(&globalContext, thisMember->entry, info, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
        }
        break;

        default:
            break;
        }
    }
    iterator_free(entryIterator);
};

void generate_code_for_type_non_generic(struct CodegenState *globalContext,
                                        struct TypeEntry *theType,
                                        struct MachineInfo *info,
                                        void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                                        void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                                        void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *))
{
    Iterator *implementedIter = NULL;
    for (implementedIter = set_begin(theType->implemented->entries); iterator_gettable(implementedIter); iterator_next(implementedIter))
    {

        struct ScopeMember *entry = iterator_get(implementedIter);
        if (entry->type != E_FUNCTION)
        {
            InternalError("Type implemented entry is not a function!\n");
        }
        struct FunctionEntry *implementedFunction = entry->entry;
        if (implementedFunction->isDefined)
        {
            char *mangledName = type_get_mangled_name(&theType->type);
            generate_code_for_function(globalContext->outFile, implementedFunction, info, mangledName, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
            free(mangledName);
        }
    }
    iterator_free(implementedIter);
}

void generate_code_for_type(struct CodegenState *globalContext,
                            struct TypeEntry *theType,
                            struct MachineInfo *info,
                            void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                            void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                            void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *))
{
    char *typeName = type_entry_name(theType);
    log(LOG_DEBUG, "Generate code for type %s", typeName);
    free(typeName);

    switch (theType->genericType)
    {
    case G_NONE:
        generate_code_for_type_non_generic(globalContext, theType, info, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
        break;

    case G_BASE:
    {
        Iterator *instanceIter = NULL;
        for (instanceIter = hash_table_begin(theType->generic.base.instances); iterator_gettable(instanceIter); iterator_next(instanceIter))
        {
            HashTableEntry *instanceEntry = iterator_get(instanceIter);
            struct TypeEntry *thisInstance = instanceEntry->value;
            generate_code_for_type_non_generic(globalContext, thisInstance, info, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
        }
        iterator_free(instanceIter);
    }
    break;

    case G_INSTANCE:
        generate_code_for_type_non_generic(globalContext, theType, info, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
        break;
    }
}

void generate_code_for_struct(struct CodegenState *globalContext,
                              struct StructDesc *theStruct,
                              struct MachineInfo *info,
                              void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                              void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                              void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *))
{

    log(LOG_DEBUG, "Generating code for struct %s", theStruct->name);

    Iterator *entryIterator = NULL;
    for (entryIterator = set_begin(theStruct->members->entries); iterator_gettable(entryIterator); iterator_next(entryIterator))
    {
        struct ScopeMember *thisMember = iterator_get(entryIterator);
        switch (thisMember->type)
        {
        case E_FUNCTION:
        {
            struct FunctionEntry *methodToGenerate = thisMember->entry;
            if (methodToGenerate->isDefined)
            {
                char *structName = theStruct->name;
                generate_code_for_function(globalContext->outFile, methodToGenerate, info, structName, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
                free(structName);
            }
        }
        break;

        default:
            break;
        }
    }
    iterator_free(entryIterator);
}

void generate_code_for_global_block(struct CodegenState *globalContext, struct Scope *globalScope, struct BasicBlock *globalBlock)
{
}

void generate_code_for_object(struct CodegenState *globalContext, struct Scope *globalScope, struct Type *type)
{
    // how to handle multidimensional arrays with intiializeArrayTo at each level? Nested labels for nested elements?
    if (type->basicType == VT_ARRAY)
    {
        InternalError("generateCodeForObject called with array type - not supported yet!\n");
    }
    else
    {
        if (type->nonArray.initializeTo != NULL)
        {
            size_t objectSize = type_get_size(type, globalScope);
            for (size_t byteIndex = 0; byteIndex < objectSize; byteIndex++)
            {
                fprintf(globalContext->outFile, "\t.byte %d\n", (type->nonArray.initializeTo)[byteIndex]);
            }
        }
        else
        {
            fprintf(globalContext->outFile, "\t.zero %zu\n", type_get_size(type, globalScope));
        }
    }
}

void generate_code_for_string_literal(struct CodegenState *globalContext, struct VariableEntry *variable, struct Scope *globalScope)
{
    if (variable->type.basicType != VT_ARRAY)
    {
        InternalError("generateCodeForStringLiteral called with non-array type!\n");
    }

    if (variable->type.array.initializeArrayTo == NULL)
    {
        InternalError("generateCodeForStringLiteral called with NULL initializeArrayTo!\n");
    }

    if (!variable->isStringLiteral)
    {
        InternalError("generateCodeForStringLiteral called with non-string-literal variable!\n");
    }

    // .section        .data
    //     .globl Counter_
    //     .type   Counter_, @object
    //     .size   Counter_, 8

    fprintf(globalContext->outFile, ".section\t.rodata\n");
    fprintf(globalContext->outFile, "\t.globl %s\n", variable->name);
    fprintf(globalContext->outFile, "\t.type %s, @object\n", variable->name);
    fprintf(globalContext->outFile, "\t.size %s, %zu\n", variable->name, type_get_size(&variable->type, globalScope));

    size_t stringLength = variable->type.array.size;
    fprintf(globalContext->outFile, "%s:\n\t.asciz \"", variable->name);
    for (size_t charIndex = 0; charIndex < stringLength; charIndex++)
    {
        fprintf(globalContext->outFile, "%c", ((char *)(variable->type.array.initializeArrayTo[charIndex]))[0]);
    }
    fprintf(globalContext->outFile, "\"\n");
}

void generate_code_for_initialized_global_array(struct CodegenState *globalContext, struct VariableEntry *variable, struct Scope *globalScope)
{
    InternalError("generate_code_for_initialized_global_array not yet supported!\n");
}

void generate_code_for_initialized_global(struct CodegenState *globalContext, struct VariableEntry *variable, struct Scope *globalScope)
{
    if (variable->type.basicType == VT_ARRAY)
    {
        InternalError("generateCodeForInitializedGlobal called with array type!\n");
    }

    if (variable->type.nonArray.initializeTo == NULL)
    {
        InternalError("generateCodeForInitializedGlobal called with NULL initializeTo!\n");
    }

    fprintf(globalContext->outFile, ".section\t.data\n");

    fprintf(globalContext->outFile, "\t.globl %s\n", variable->name);
    fprintf(globalContext->outFile, "\t.type %s, @object\n", variable->name);
    fprintf(globalContext->outFile, "\t.size %s, %zu\n", variable->name, type_get_size(&variable->type, globalScope));
    fprintf(globalContext->outFile, "%s:\n", variable->name);

    size_t objectSize = type_get_size(&variable->type, NULL);
    for (size_t byteIndex = 0; byteIndex < objectSize; byteIndex++)
    {
        fprintf(globalContext->outFile, "\t.byte %d\n", (variable->type.nonArray.initializeTo)[byteIndex]);
    }
}

void generate_code_for_uninitialized_global(struct CodegenState *globalContext, struct VariableEntry *variable, struct Scope *globalScope)
{
    fprintf(globalContext->outFile, ".section\t.bss\n");

    fprintf(globalContext->outFile, ".comm %s, %zu, 4\n", variable->name, type_get_size(&variable->type, globalScope));
}

void generate_code_for_global_variable(struct CodegenState *globalContext, struct Scope *globalScope, struct VariableEntry *variable)
{
    // early return if the variable is declared as extern, don't emit any code for it
    if (variable->isExtern)
    {
        return;
    }

    if (variable->type.basicType == VT_ARRAY)
    {
        // string literals go in rodata
        if ((variable->type.array.initializeArrayTo != NULL))
        {
            if (variable->isStringLiteral)
            {
                generate_code_for_string_literal(globalContext, variable, globalScope);
            }
            else
            {
                generate_code_for_initialized_global_array(globalContext, variable, globalScope);
            }
        }
        else
        {
            generate_code_for_uninitialized_global(globalContext, variable, globalScope);
        }
    }
    else
    {
        if (variable->type.nonArray.initializeTo != NULL)
        {
            generate_code_for_initialized_global(globalContext, variable, globalScope);
        }
        else
        {
            generate_code_for_uninitialized_global(globalContext, variable, globalScope);
        }
    }

    fprintf(globalContext->outFile, ".section .text\n");
}

/*
 * code generation for funcitons (lifetime management, etc)
 *
 */
extern struct Config config;
void generate_code_for_function(FILE *outFile,
                                struct FunctionEntry *function,
                                struct MachineInfo *info,
                                char *methodOfStructName,
                                void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                                void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                                void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *))
{
    char *fullFunctionName = function->name;
    if (methodOfStructName != NULL)
    {
        // TODO: method/associated function name mangling/uniqueness
        fullFunctionName = malloc(strlen(function->name) + strlen(methodOfStructName) + 2);
        strcpy(fullFunctionName, methodOfStructName);
        strcat(fullFunctionName, "_");
        strcat(fullFunctionName, function->name);
        log(LOG_DEBUG, "the real name of %s is %s", function->name, fullFunctionName);
    }
    size_t instructionIndex = 0; // index from start of function in terms of number of instructions
    struct CodegenState state;
    state.outFile = outFile;
    state.instructionIndex = &instructionIndex;

    log(LOG_INFO, "Generate code for function %s", fullFunctionName);

    fprintf(outFile, ".globl %s\n", fullFunctionName);
    fprintf(outFile, ".type %s, @function\n", fullFunctionName);

    fprintf(outFile, ".align 2\n%s:\n", fullFunctionName);
    fprintf(outFile, "\t.loc 1 %d %d\n", function->correspondingTree.sourceLine, function->correspondingTree.sourceCol);

    // TODO: debug symbols for asm functions?
    if (function->isAsmFun)
    {
        log(LOG_DEBUG, "%s is an asm function", function->name);
    }

    emitPrologue(&state, &function->regalloc, info);

    if (function->isAsmFun && (function->BasicBlockList->size != 2))
    {
        InternalError("Asm function with %zu basic blocks seen - expected 2!", function->BasicBlockList->size);
    }

    Iterator *argIterator = NULL;
    for (argIterator = deque_front(function->arguments); iterator_gettable(argIterator); iterator_next(argIterator))
    {
        struct VariableEntry *examinedArgument = iterator_get(argIterator);
        struct Lifetime *argLifetime = lifetime_find_by_name(function->regalloc.allLifetimes, examinedArgument->name);
        if (argLifetime->wbLocation == WB_REGISTER)
        {
            argLifetime->writebackInfo.regLocation->containedLifetime = argLifetime;
        }
    }
    iterator_free(argIterator);

    Iterator *blockRunner = NULL;
    for (blockRunner = list_begin(function->BasicBlockList); iterator_gettable(blockRunner); iterator_next(blockRunner))
    {
        struct BasicBlock *block = iterator_get(blockRunner);
        generateCodeForBasicBlock(&state, &function->regalloc, info, block, fullFunctionName);
    }
    iterator_free(blockRunner);

    emitEpilogue(&state, &function->regalloc, info, fullFunctionName);

    if (methodOfStructName != NULL)
    {
        free(fullFunctionName);
    }
}
