use crate::backend::arch::generic::{RegisterPurpose::*, RegisterSaveConvention::*};

use super::*;

pub fn rv64g_registers() -> ArchitectureRegisters {
    let mut architecture_registers = ArchitectureRegisters::new();

    let riscv_registers = vec![
        Register::new("zero", Zero, NoSave),
        Register::new("ra", ReturnAddress, CallerSave),
        Register::new("sp", StackPointer, CalleeSave),
        Register::new("gp", Other, NoSave),
        Register::new("tp", Other, NoSave),
        Register::new("t0", Temporary, CallerSave),
        Register::new("t1", Temporary, CallerSave),
        Register::new("t2", Temporary, CallerSave),
        Register::new("fp", FramePointer, CalleeSave),
        Register::new("s1", GeneralPurpose, CalleeSave),
        Register::new("a0", Argument, CallerSave),
        Register::new("a1", Argument, CallerSave),
        Register::new("a2", Argument, CallerSave),
        Register::new("a3", Argument, CallerSave),
        Register::new("a4", Argument, CallerSave),
        Register::new("a5", Argument, CallerSave),
        Register::new("a6", Argument, CallerSave),
        Register::new("a7", Argument, CallerSave),
        Register::new("s2", GeneralPurpose, CalleeSave),
        Register::new("s3", GeneralPurpose, CalleeSave),
        Register::new("s4", GeneralPurpose, CalleeSave),
        Register::new("s5", GeneralPurpose, CalleeSave),
        Register::new("s6", GeneralPurpose, CalleeSave),
        Register::new("s7", GeneralPurpose, CalleeSave),
        Register::new("s8", GeneralPurpose, CalleeSave),
        Register::new("s9", GeneralPurpose, CalleeSave),
        Register::new("s10", GeneralPurpose, CalleeSave),
        Register::new("s11", GeneralPurpose, CalleeSave),
        Register::new("t3", Temporary, CallerSave),
        Register::new("t4", Temporary, CallerSave),
        Register::new("t5", Temporary, CallerSave),
        Register::new("t6", Temporary, CallerSave),
    ];

    for register in riscv_registers {
        architecture_registers.add(register);
    }

    architecture_registers
}
