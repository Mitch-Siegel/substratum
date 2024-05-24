#include "regalloc_riscv.h"

#include <string.h>

struct Register riscvRegisters[RISCV_REGISTER_COUNT] = {
    [zero]
    { NULL, zero, "zero"},
    [ra]
    { NULL, ra, "ra"},
    [sp]
    { NULL, sp, "sp"},
    [gp]
    { NULL, gp, "gp"},
    [tp]
    { NULL, tp, "tp"},
    [t0]
    { NULL, t0, "t0"},
    [t1]
    { NULL, t1, "t1"},
    [t2]
    { NULL, t2, "t2"},
    [fp]
    { NULL, fp, "fp"},
    [s1]
    { NULL, s1, "s1"},
    [a0]
    { NULL, a0, "a0"},
    [a1]
    { NULL, a1, "a1"},
    [a2]
    { NULL, a2, "a2"},
    [a3]
    { NULL, a3, "a3"},
    [a4]
    { NULL, a4, "a4"},
    [a5]
    { NULL, a5, "a5"},
    [a6]
    { NULL, a6, "a6"},
    [a7]
    { NULL, a7, "a7"},
    [s2]
    { NULL, s2, "s2"},
    [s3]
    { NULL, s3, "s3"},
    [s4]
    { NULL, s4, "s4"},
    [s5]
    { NULL, s5, "s5"},
    [s6]
    { NULL, s6, "s6"},
    [s7]
    { NULL, s7, "s7"},
    [s8]
    { NULL, s8, "s8"},
    [s9]
    { NULL, s9, "s9"},
    [s10]
    { NULL, s10, "s10"},
    [s11]
    { NULL, s11, "s11"},
    [t3]
    { NULL, t3, "t3"},
    [t4]
    { NULL, t4, "t4"},
    [t5]
    { NULL, t5, "t5"},
    [t6]
    { NULL, t6, "t6"},
};

struct MachineContext *setupRiscvMachineContext()
{
    struct MachineContext *context = malloc(sizeof(struct MachineContext));
    memset(context, 0, sizeof(struct MachineContext));
    context->maxReg = RISCV_REGISTER_COUNT;

    context->returnAddress = &riscvRegisters[ra];
    context->stackPointer = &riscvRegisters[sp];
    context->framePointer = &riscvRegisters[fp];

    context->temps[0] = &riscvRegisters[t0];
    context->temps[1] = &riscvRegisters[t1];
    context->temps[2] = &riscvRegisters[t2];

    const u8 numArgumentRegisters = 8;
    context->arguments = malloc(numArgumentRegisters * sizeof(struct Register *));
    context->n_arguments = numArgumentRegisters;
    for (u8 argReg = 0; argReg < numArgumentRegisters; argReg++)
    {
        context->arguments[argReg] = &riscvRegisters[a0 + argReg];
    }

    const u8 nCalleeSaved = 11;
    context->callee_save = malloc(nCalleeSaved * sizeof(struct Register *));
    context->n_caller_save = nCalleeSaved;
    for (u8 calleeReg = 0; calleeReg < nCalleeSaved; calleeReg++)
    {
        context->callee_save[calleeReg] = &riscvRegisters[s1 + calleeReg];
    }

    const u8 nCallerSaved = 4;
    context->caller_save = malloc(nCallerSaved * sizeof(struct Register *));
    context->n_caller_save = nCallerSaved;
    for (u8 callerReg = 0; callerReg < nCallerSaved; callerReg++)
    {
        context->caller_save[callerReg] = &riscvRegisters[t3 + callerReg];
    }

    context->allRegisters = malloc(RISCV_REGISTER_COUNT * sizeof(struct Register *));
    for(u8 regIndex = 0; regIndex < RISCV_REGISTER_COUNT; regIndex++)
    {
        context->allRegisters[regIndex] = &riscvRegisters[regIndex];
    }

    return context;
}