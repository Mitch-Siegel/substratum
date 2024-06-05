#include "regalloc_riscv.h"

#include <string.h>

struct Register riscvRegisters[RISCV_REGISTER_COUNT] = {
    [zero]
    { NULL, zero, "zero" },
    [ra]
    { NULL, ra, "ra" },
    [sp]
    { NULL, sp, "sp" },
    [gp]
    { NULL, gp, "gp" },
    [tp]
    { NULL, tp, "tp" },
    [t0]
    { NULL, t0, "t0" },
    [t1]
    { NULL, t1, "t1" },
    [t2]
    { NULL, t2, "t2" },
    [fp]
    { NULL, fp, "fp" },
    [s1]
    { NULL, s1, "s1" },
    [a0]
    { NULL, a0, "a0" },
    [a1]
    { NULL, a1, "a1" },
    [a2]
    { NULL, a2, "a2" },
    [a3]
    { NULL, a3, "a3" },
    [a4]
    { NULL, a4, "a4" },
    [a5]
    { NULL, a5, "a5" },
    [a6]
    { NULL, a6, "a6" },
    [a7]
    { NULL, a7, "a7" },
    [s2]
    { NULL, s2, "s2" },
    [s3]
    { NULL, s3, "s3" },
    [s4]
    { NULL, s4, "s4" },
    [s5]
    { NULL, s5, "s5" },
    [s6]
    { NULL, s6, "s6" },
    [s7]
    { NULL, s7, "s7" },
    [s8]
    { NULL, s8, "s8" },
    [s9]
    { NULL, s9, "s9" },
    [s10]
    { NULL, s10, "s10" },
    [s11]
    { NULL, s11, "s11" },
    [t3]
    { NULL, t3, "t3" },
    [t4]
    { NULL, t4, "t4" },
    [t5]
    { NULL, t5, "t5" },
    [t6]
    { NULL, t6, "t6" },
};

// TODO: flip ordering so allocation goes from low indices to high (and fix resulting bugs?)
struct MachineInfo *riscv_SetupMachineInfo()
{
    const u8 nTemps = 3;
    const u8 nArguments = 8;
    const u8 nGeneralPurpose = 15;
    const u8 nNoSave = 0;
    const u8 nCalleeSave = 12;
    const u8 nCallerSave = 12;
    struct MachineInfo *info = MachineInfo_New(RISCV_REGISTER_COUNT, nTemps, nArguments, nGeneralPurpose, nNoSave, nCalleeSave, nCallerSave);

    info->returnAddress = &riscvRegisters[ra];
    info->stackPointer = &riscvRegisters[sp];
    info->framePointer = &riscvRegisters[fp];
    info->returnValue = &riscvRegisters[a0];

    info->temps[0] = &riscvRegisters[t0];
    info->temps[1] = &riscvRegisters[t1];
    info->temps[2] = &riscvRegisters[t2];

    u8 argReg = 0;
    info->arguments[argReg++] = &riscvRegisters[a7];
    info->arguments[argReg++] = &riscvRegisters[a6];
    info->arguments[argReg++] = &riscvRegisters[a5];
    info->arguments[argReg++] = &riscvRegisters[a4];
    info->arguments[argReg++] = &riscvRegisters[a3];
    info->arguments[argReg++] = &riscvRegisters[a2];
    info->arguments[argReg++] = &riscvRegisters[a1];
    info->arguments[argReg++] = &riscvRegisters[a0];

    u8 gpReg = 0;
    info->generalPurpose[gpReg++] = &riscvRegisters[t6];
    info->generalPurpose[gpReg++] = &riscvRegisters[t5];
    info->generalPurpose[gpReg++] = &riscvRegisters[t4];
    info->generalPurpose[gpReg++] = &riscvRegisters[t3];

    info->generalPurpose[gpReg++] = &riscvRegisters[s11];
    info->generalPurpose[gpReg++] = &riscvRegisters[s10];
    info->generalPurpose[gpReg++] = &riscvRegisters[s9];
    info->generalPurpose[gpReg++] = &riscvRegisters[s8];
    info->generalPurpose[gpReg++] = &riscvRegisters[s7];
    info->generalPurpose[gpReg++] = &riscvRegisters[s6];
    info->generalPurpose[gpReg++] = &riscvRegisters[s5];
    info->generalPurpose[gpReg++] = &riscvRegisters[s4];
    info->generalPurpose[gpReg++] = &riscvRegisters[s3];
    info->generalPurpose[gpReg++] = &riscvRegisters[s2];
    info->generalPurpose[gpReg++] = &riscvRegisters[s1];

    u8 calleeReg = 0;
    // don't actually mark sp and fp as callee-save as they are handled specifically by emitprologue and emitepilogue to get the ordering correct
    info->callee_save[calleeReg++] = &riscvRegisters[s11];
    info->callee_save[calleeReg++] = &riscvRegisters[s10];
    info->callee_save[calleeReg++] = &riscvRegisters[s9];
    info->callee_save[calleeReg++] = &riscvRegisters[s8];
    info->callee_save[calleeReg++] = &riscvRegisters[s7];
    info->callee_save[calleeReg++] = &riscvRegisters[s6];
    info->callee_save[calleeReg++] = &riscvRegisters[s5];
    info->callee_save[calleeReg++] = &riscvRegisters[s4];
    info->callee_save[calleeReg++] = &riscvRegisters[s3];
    info->callee_save[calleeReg++] = &riscvRegisters[s2];
    info->callee_save[calleeReg++] = &riscvRegisters[s1];
    info->callee_save[calleeReg++] = &riscvRegisters[ra];

    u8 callerReg = 0;

    info->caller_save[callerReg++] = &riscvRegisters[a7];
    info->caller_save[callerReg++] = &riscvRegisters[a6];
    info->caller_save[callerReg++] = &riscvRegisters[a3];
    info->caller_save[callerReg++] = &riscvRegisters[a5];
    info->caller_save[callerReg++] = &riscvRegisters[a4];
    info->caller_save[callerReg++] = &riscvRegisters[a2];
    info->caller_save[callerReg++] = &riscvRegisters[a1];
    info->caller_save[callerReg++] = &riscvRegisters[a0];

    info->caller_save[callerReg++] = &riscvRegisters[t6];
    info->caller_save[callerReg++] = &riscvRegisters[t5];
    info->caller_save[callerReg++] = &riscvRegisters[t4];
    info->caller_save[callerReg++] = &riscvRegisters[t3];

    for (i32 regIndex = 0; regIndex < RISCV_REGISTER_COUNT; regIndex++)
    {
        info->allRegisters[regIndex] = &riscvRegisters[regIndex];
    }

    return info;
}