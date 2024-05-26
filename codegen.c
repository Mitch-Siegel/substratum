#include "codegen.h"

#include "codegen_generic.h"
#include "log.h"
#include "regalloc.h"
#include "regalloc_riscv.h"
#include "symtab.h"

void generateCodeForProgram(struct SymbolTable *table,
                            FILE *outFile,
                            void (*emitPrologue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *),
                            void (*emitEpilogue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, char *),
                            void (*generateCodeForBasicBlock)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, struct BasicBlock *, char *))
{
    struct CodegenState globalContext;
    size_t globalInstructionIndex = 0;
    globalContext.instructionIndex = &globalInstructionIndex;
    globalContext.outFile = outFile;

    // fprintf(outFile, "\t.text\n");
    for (size_t entryIndex = 0; entryIndex < table->globalScope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = table->globalScope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case e_function:
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

            generateCodeForFunction(outFile, generatedFunction, NULL, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
            fprintf(outFile, "\t.size %s, .-%s\n", generatedFunction->name, generatedFunction->name);
        }
        break;

        case e_basicblock:
        {
            generateCodeForGlobalBlock(&globalContext, table->globalScope, thisMember->entry);
        }
        break;

        case e_variable:
        {
            generateCodeForGlobalVariable(&globalContext, table->globalScope, thisMember->entry);
        }
        break;

        case e_struct:
        {
            generateCodeForStruct(&globalContext, thisMember->entry, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
        }
        break;

        default:
            break;
        }
    }
};

void generateCodeForStruct(struct CodegenState *globalContext,
                           struct StructEntry *theStruct,
                           void (*emitPrologue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *),
                           void (*emitEpilogue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, char *),
                           void (*generateCodeForBasicBlock)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, struct BasicBlock *, char *))
{
    for (size_t entryIndex = 0; entryIndex < theStruct->members->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = theStruct->members->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case e_function:
        {
            struct FunctionEntry *methodToGenerate = thisMember->entry;
            if (methodToGenerate->isDefined)
            {
                generateCodeForFunction(globalContext->outFile, methodToGenerate, theStruct->name, emitPrologue, emitEpilogue, generateCodeForBasicBlock);
            }
        }
        break;

        default:
            break;
        }
    }
}

void generateCodeForGlobalBlock(struct CodegenState *globalContext, struct Scope *globalScope, struct BasicBlock *globalBlock)
{
}

void generateCodeForObject(struct CodegenState *globalContext, struct Scope *globalScope, struct Type *type)
{
    // how to handle multidimensional arrays with intiializeArrayTo at each level? Nested labels for nested elements?
    if (type->basicType == vt_array)
    {
        InternalError("generateCodeForObject called with array type - not supported yet!\n");
    }
    else
    {
        if (type->nonArray.initializeTo != NULL)
        {
            size_t objectSize = Type_GetSize(type, globalScope);
            for (size_t byteIndex = 0; byteIndex < objectSize; byteIndex++)
            {
                fprintf(globalContext->outFile, "\t.byte %d\n", (type->nonArray.initializeTo)[byteIndex]);
            }
        }
        else
        {
            fprintf(globalContext->outFile, "\t.zero %zu\n", Type_GetSize(type, globalScope));
        }
    }
}

void generateCodeForGlobalVariable(struct CodegenState *globalContext, struct Scope *globalScope, struct VariableEntry *variable)
{
    // early return if the variable is declared as extern, don't emit any code for it
    if (variable->isExtern)
    {
        return;
    }

    char *varName = variable->name;
    size_t varSize = Type_GetSize(&variable->type, globalScope);

    if (variable->type.basicType == vt_array)
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

    u8 alignBits = Type_GetAlignment(&variable->type, globalScope);
    if (alignBits > 0)
    {
        fprintf(globalContext->outFile, ".align %d\n", alignBits);
    }

    fprintf(globalContext->outFile, "\t.type\t%s, @object\n", varName);
    fprintf(globalContext->outFile, "\t.size \t%s, %zu\n", varName, varSize);
    fprintf(globalContext->outFile, "%s:\n", varName);

    if (variable->type.basicType == vt_array)
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
                generateCodeForObject(globalContext, globalScope, &variable->type);
            }
        }
    }
    else
    {
        generateCodeForObject(globalContext, globalScope, &variable->type);
    }

    fprintf(globalContext->outFile, ".section .text\n");
}

/*
 * code generation for funcitons (lifetime management, etc)
 *
 */
extern struct Config config;
void generateCodeForFunction(FILE *outFile, struct FunctionEntry *function,
                             char *methodOfStructName,
                             void (*emitPrologue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *),
                             void (*emitEpilogue)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, char *),
                             void (*generateCodeForBasicBlock)(struct CodegenState *, struct CodegenMetadata *, struct MachineInfo *, struct BasicBlock *, char *))
{
    char *fullFunctionName = function->name;
    if (methodOfStructName != NULL)
    {
        // TODO: member function name mangling/uniqueness
        fullFunctionName = malloc(strlen(function->name) + strlen(methodOfStructName) + 2);
        strcpy(fullFunctionName, methodOfStructName);
        strcat(fullFunctionName, "_");
        strcat(fullFunctionName, function->name);
        printf("the real name of %s is %s\n", function->name, fullFunctionName);
    }
    size_t instructionIndex = 0; // index from start of function in terms of number of instructions
    struct CodegenState state;
    state.outFile = outFile;
    state.instructionIndex = &instructionIndex;

    Log(LOG_INFO, "Generate code for function %s", fullFunctionName);

    fprintf(outFile, ".globl %s\n", fullFunctionName);
    fprintf(outFile, ".type %s, @function\n", fullFunctionName);

    fprintf(outFile, ".align 2\n%s:\n", fullFunctionName);
    fprintf(outFile, "\t.loc 1 %d %d\n", function->correspondingTree.sourceLine, function->correspondingTree.sourceCol);

    struct CodegenMetadata metadata;
    memset(&metadata, 0, sizeof(struct CodegenMetadata));

    setupMachineInfo = setupRiscvMachineInfo;

    metadata.function = function;
    metadata.scope = function->mainScope;

    struct MachineInfo *info = setupMachineInfo();
    allocateRegisters(&metadata, info);

    // TODO: debug symbols for asm functions?
    if (function->isAsmFun)
    {
        Log(LOG_DEBUG, "%s is an asm function", function->name);
    }

    emitPrologue(&state, &metadata, info);

    if (function->isAsmFun && (function->BasicBlockList->size != 1))
    {
        InternalError("Asm function with %zu basic blocks seen - expected 1!", function->BasicBlockList->size);
    }

    for (struct LinkedListNode *blockRunner = function->BasicBlockList->head; blockRunner != NULL; blockRunner = blockRunner->next)
    {
        struct BasicBlock *block = blockRunner->data;
        Log(LOG_DEBUG, "Generating code for basic block %zd", block->labelNum);
        generateCodeForBasicBlock(&state, &metadata, info, block, fullFunctionName);
    }

    emitEpilogue(&state, &metadata, info, fullFunctionName);

    MachineInfo_Free(info);
    Set_Free(metadata.touchedRegisters);

    // clean up after ourselves
    Set_Free(metadata.allLifetimes);

    if (methodOfStructName != NULL)
    {
        free(fullFunctionName);
    }
}
