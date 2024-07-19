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
                               void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *))
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

            // TODO: genericize this/sort out baremetal vs executable stuff
            if (!strcmp(generatedFunction->name, "main"))
            {
                fprintf(outFile, "\t.globl _start\n_start:\n\tli sp, 0x81000000\n\tcall main\n\tpgm_done:\n\twfi\n\tj pgm_done\n");
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

        case E_STRUCT:
        {
            generate_code_for_struct(&globalContext, thisMember->entry, info, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
        }
        break;

        default:
            break;
        }
    }
    iterator_free(entryIterator);
};

void generate_code_for_struct(struct CodegenState *globalContext,
                              struct StructEntry *theStruct,
                              struct MachineInfo *info,
                              void (*emitPrologue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *),
                              void (*emitEpilogue)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, char *),
                              void (*generateCodeForBasicBlock)(struct CodegenState *, struct RegallocMetadata *, struct MachineInfo *, struct BasicBlock *, char *))
{
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
                generate_code_for_function(globalContext->outFile, methodToGenerate, info, theStruct->name, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
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

void generate_code_for_global_variable(struct CodegenState *globalContext, struct Scope *globalScope, struct VariableEntry *variable)
{
    // early return if the variable is declared as extern, don't emit any code for it
    if (variable->isExtern)
    {
        return;
    }

    char *varName = variable->name;
    size_t varSize = type_get_size(&variable->type, globalScope);

    if (variable->type.basicType == VT_ARRAY)
    {
        // string literals go in rodata
        if ((variable->type.array.initializeArrayTo != NULL) && (variable->isStringLiteral))
        {
            fprintf(globalContext->outFile, ".section\t.rodata\n");
        }
        fprintf(globalContext->outFile, ".section\t.data\n");
    }
    else
    {
        fprintf(globalContext->outFile, ".section\t.bss\n");
    }

    fprintf(globalContext->outFile, "\t.globl %s\n", varName);

    u8 alignBits = type_get_alignment(&variable->type, globalScope);
    if (alignBits > 0)
    {
        fprintf(globalContext->outFile, ".align %d\n", alignBits);
    }

    fprintf(globalContext->outFile, "\t.type\t%s, @object\n", varName);
    fprintf(globalContext->outFile, "\t.size \t%s, %zu\n", varName, varSize);
    fprintf(globalContext->outFile, "%s:\n", varName);

    if (variable->type.basicType == VT_ARRAY)
    {
        if (variable->type.array.initializeArrayTo != NULL)
        {
            if (variable->isStringLiteral)
            {
                fprintf(globalContext->outFile, "\t.asciz \"");
                for (size_t charIndex = 0; charIndex < variable->type.array.size; charIndex++)
                {
                    fprintf(globalContext->outFile, "%c", ((char *)variable->type.array.initializeArrayTo[charIndex])[0]);
                }
                fprintf(globalContext->outFile, "\"\n");
            }
            else
            {
                generate_code_for_object(globalContext, globalScope, &variable->type);
            }
        }
    }
    else
    {
        generate_code_for_object(globalContext, globalScope, &variable->type);
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

    if (function->isAsmFun && (function->BasicBlockList->size != 1))
    {
        InternalError("Asm function with %zu basic blocks seen - expected 1!", function->BasicBlockList->size);
    }

    Iterator *argIterator = NULL;
    for (argIterator = deque_front(function->arguments); iterator_gettable(argIterator); iterator_next(argIterator))
    {
        struct VariableEntry *examinedArgument = iterator_get(argIterator);
        struct Lifetime *argLifetime = lifetime_find(function->regalloc.allLifetimes, examinedArgument->name);
        argLifetime->writebackInfo.regLocation->containedLifetime = argLifetime;
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
