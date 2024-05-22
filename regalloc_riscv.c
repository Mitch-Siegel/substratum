#include "regalloc_riscv.h"

#include <string.h>

char *riscvRegisterNames[RISCV_REGISTER_COUNT] = {
    [zero] "zero",
    [ra] "ra",
    [sp] "sp",
    [gp] "gp",
    [tp] "tp",
    [t0] "t0",
    [t1] "t1",
    [t2] "t2",
    [fp] "fp",
    [s1] "s1",
    [a0] "a0",
    [a1] "a1",
    [a2] "a2",
    [a3] "a3",
    [a4] "a4",
    [a5] "a5",
    [a6] "a6",
    [a7] "a7",
    [s2] "s2",
    [s3] "s3",
    [s4] "s4",
    [s5] "s5",
    [s6] "s6",
    [s7] "s7",
    [s8] "s8",
    [s9] "s9",
    [s10] "s10",
    [s11] "s11",
    [t3] "t3",
    [t4] "t4",
    [t5] "t5",
    [t6] "t6",
};

struct Register riscvRegisters[RISCV_REGISTER_COUNT] = {
    [zero]
    { NULL, zero },
    [ra]
    { NULL, ra },
    [sp]
    { NULL, sp },
    [gp]
    { NULL, gp },
    [tp]
    { NULL, tp },
    [t0]
    { NULL, t0 },
    [t1]
    { NULL, t1 },
    [t2]
    { NULL, t2 },
    [fp]
    { NULL, fp },
    [s1]
    { NULL, s1 },
    [a0]
    { NULL, a0 },
    [a1]
    { NULL, a1 },
    [a2]
    { NULL, a2 },
    [a3]
    { NULL, a3 },
    [a4]
    { NULL, a4 },
    [a5]
    { NULL, a5 },
    [a6]
    { NULL, a6 },
    [a7]
    { NULL, a7 },
    [s2]
    { NULL, s2 },
    [s3]
    { NULL, s3 },
    [s4]
    { NULL, s4 },
    [s5]
    { NULL, s5 },
    [s6]
    { NULL, s6 },
    [s7]
    { NULL, s7 },
    [s8]
    { NULL, s8 },
    [s9]
    { NULL, s9 },
    [s10]
    { NULL, s10 },
    [s11]
    { NULL, s11 },
    [t3]
    { NULL, t3 },
    [t4]
    { NULL, t4 },
    [t5]
    { NULL, t5 },
    [t6]
    { NULL, t6 },
};

struct MachineContext *setupRiscvMachineContext()
{
    struct MachineContext *context = malloc(sizeof(struct MachineContext));
    memset(context, 0, sizeof(struct MachineContext));
    context->registerNames = riscvRegisterNames;
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
    context->arguments[0] = &riscvRegisters[a0];
    context->arguments[1] = &riscvRegisters[a1];
    context->arguments[2] = &riscvRegisters[a2];
    context->arguments[3] = &riscvRegisters[a3];
    context->arguments[4] = &riscvRegisters[a4];
    context->arguments[5] = &riscvRegisters[a5];
    context->arguments[6] = &riscvRegisters[a6];
    context->arguments[7] = &riscvRegisters[a7];

    const u8 nCalleeSaved = 12;
    context->callee_save = malloc(nCalleeSaved * sizeof(struct Register *));
    context->n_callee_save = nCalleeSaved;
    context->callee_save[0] = &riscvRegisters[s1];
    context->callee_save[1] = &riscvRegisters[s2];
    context->callee_save[2] = &riscvRegisters[s3];
    context->callee_save[3] = &riscvRegisters[s4];
    context->callee_save[4] = &riscvRegisters[s5];
    context->callee_save[5] = &riscvRegisters[s6];
    context->callee_save[6] = &riscvRegisters[s7];
    context->callee_save[8] = &riscvRegisters[s8];
    context->callee_save[9] = &riscvRegisters[s9];
    context->callee_save[10] = &riscvRegisters[s10];
    context->callee_save[11] = &riscvRegisters[s11];

    const u8 nCallerSaved = 4;
    context->caller_save = malloc(nCallerSaved * sizeof(struct Register *));
    context->n_caller_save = nCallerSaved;
    context->caller_save[0] = &riscvRegisters[t3];
    context->caller_save[1] = &riscvRegisters[t4];
    context->caller_save[2] = &riscvRegisters[t5];
    context->caller_save[3] = &riscvRegisters[t6];


    return context;
}