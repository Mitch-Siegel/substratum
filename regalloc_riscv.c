#include "regalloc_riscv.h"

#include <string.h>

struct Register riscvRegisters[RISCV_REGISTER_COUNT] = {
    [ZERO]
    { NULL, ZERO, "zero" },
    [RA]
    { NULL, RA, "ra" },
    [SP]
    { NULL, SP, "sp" },
    [GP]
    { NULL, GP, "gp" },
    [TP]
    { NULL, TP, "tp" },
    [T0]
    { NULL, T0, "t0" },
    [T1]
    { NULL, T1, "t1" },
    [T2]
    { NULL, T2, "t2" },
    [FP]
    { NULL, FP, "fp" },
    [S1]
    { NULL, S1, "s1" },
    [A0]
    { NULL, A0, "a0" },
    [A1]
    { NULL, A1, "a1" },
    [A2]
    { NULL, A2, "a2" },
    [A3]
    { NULL, A3, "a3" },
    [A4]
    { NULL, A4, "a4" },
    [A5]
    { NULL, A5, "a5" },
    [A6]
    { NULL, A6, "a6" },
    [A7]
    { NULL, A7, "a7" },
    [S2]
    { NULL, S2, "s2" },
    [S3]
    { NULL, S3, "s3" },
    [S4]
    { NULL, S4, "s4" },
    [S5]
    { NULL, S5, "s5" },
    [S6]
    { NULL, S6, "s6" },
    [S7]
    { NULL, S7, "s7" },
    [S8]
    { NULL, S8, "s8" },
    [S9]
    { NULL, S9, "s9" },
    [S10]
    { NULL, S10, "s10" },
    [S11]
    { NULL, S11, "s11" },
    [T3]
    { NULL, T3, "t3" },
    [T4]
    { NULL, T4, "t4" },
    [T5]
    { NULL, T5, "t5" },
    [T6]
    { NULL, T6, "t6" },
};

// TODO: flip ordering so allocation goes from low indices to high (and fix resulting bugs?)
struct MachineInfo *riscv_setup_machine_info()
{
    const u8 N_TEMPS = 3;
    const u8 N_ARGUMENTS = 8;
    const u8 N_GENERAL_PURPOSE = 15;
    const u8 N_NO_SAVE = 0;
    const u8 N_CALLEE_SAVE = 12;
    const u8 N_CALLER_SAVE = 12;
    struct MachineInfo *info = machine_info_new(RISCV_REGISTER_COUNT, N_TEMPS, N_ARGUMENTS, N_GENERAL_PURPOSE, N_NO_SAVE, N_CALLEE_SAVE, N_CALLER_SAVE);

    info->returnAddress = &riscvRegisters[RA];
    info->stackPointer = &riscvRegisters[SP];
    info->framePointer = &riscvRegisters[FP];
    info->returnValue = &riscvRegisters[A0];

    info->temps[0] = &riscvRegisters[T0];
    info->temps[1] = &riscvRegisters[T1];
    info->temps[2] = &riscvRegisters[T2];

    u8 argReg = 0;
    info->arguments[argReg++] = &riscvRegisters[A7];
    info->arguments[argReg++] = &riscvRegisters[A6];
    info->arguments[argReg++] = &riscvRegisters[A5];
    info->arguments[argReg++] = &riscvRegisters[A4];
    info->arguments[argReg++] = &riscvRegisters[A3];
    info->arguments[argReg++] = &riscvRegisters[A2];
    info->arguments[argReg++] = &riscvRegisters[A1];
    info->arguments[argReg++] = &riscvRegisters[A0];

    u8 gpReg = 0;
    info->generalPurpose[gpReg++] = &riscvRegisters[T6];
    info->generalPurpose[gpReg++] = &riscvRegisters[T5];
    info->generalPurpose[gpReg++] = &riscvRegisters[T4];
    info->generalPurpose[gpReg++] = &riscvRegisters[T3];

    info->generalPurpose[gpReg++] = &riscvRegisters[S11];
    info->generalPurpose[gpReg++] = &riscvRegisters[S10];
    info->generalPurpose[gpReg++] = &riscvRegisters[S9];
    info->generalPurpose[gpReg++] = &riscvRegisters[S8];
    info->generalPurpose[gpReg++] = &riscvRegisters[S7];
    info->generalPurpose[gpReg++] = &riscvRegisters[S6];
    info->generalPurpose[gpReg++] = &riscvRegisters[S5];
    info->generalPurpose[gpReg++] = &riscvRegisters[S4];
    info->generalPurpose[gpReg++] = &riscvRegisters[S3];
    info->generalPurpose[gpReg++] = &riscvRegisters[S2];
    info->generalPurpose[gpReg++] = &riscvRegisters[S1];

    u8 calleeReg = 0;
    // don't actually mark sp and fp as callee-save as they are handled specifically by emitprologue and emitepilogue to get the ordering correct
    info->callee_save[calleeReg++] = &riscvRegisters[S11];
    info->callee_save[calleeReg++] = &riscvRegisters[S10];
    info->callee_save[calleeReg++] = &riscvRegisters[S9];
    info->callee_save[calleeReg++] = &riscvRegisters[S8];
    info->callee_save[calleeReg++] = &riscvRegisters[S7];
    info->callee_save[calleeReg++] = &riscvRegisters[S6];
    info->callee_save[calleeReg++] = &riscvRegisters[S5];
    info->callee_save[calleeReg++] = &riscvRegisters[S4];
    info->callee_save[calleeReg++] = &riscvRegisters[S3];
    info->callee_save[calleeReg++] = &riscvRegisters[S2];
    info->callee_save[calleeReg++] = &riscvRegisters[S1];
    info->callee_save[calleeReg++] = &riscvRegisters[RA];

    u8 callerReg = 0;

    info->caller_save[callerReg++] = &riscvRegisters[A7];
    info->caller_save[callerReg++] = &riscvRegisters[A6];
    info->caller_save[callerReg++] = &riscvRegisters[A3];
    info->caller_save[callerReg++] = &riscvRegisters[A5];
    info->caller_save[callerReg++] = &riscvRegisters[A4];
    info->caller_save[callerReg++] = &riscvRegisters[A2];
    info->caller_save[callerReg++] = &riscvRegisters[A1];
    info->caller_save[callerReg++] = &riscvRegisters[A0];

    info->caller_save[callerReg++] = &riscvRegisters[T6];
    info->caller_save[callerReg++] = &riscvRegisters[T5];
    info->caller_save[callerReg++] = &riscvRegisters[T4];
    info->caller_save[callerReg++] = &riscvRegisters[T3];

    for (i32 regIndex = 0; regIndex < RISCV_REGISTER_COUNT; regIndex++)
    {
        info->allRegisters[regIndex] = &riscvRegisters[regIndex];
    }

    return info;
}