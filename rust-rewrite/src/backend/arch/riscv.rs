use crate::backend::arch::generic::*;
pub mod registers_riscv;
use crate::midend;
pub use registers_riscv::*;

pub struct RV64G {}

impl TargetArchitecture for RV64G {
    fn word_size() -> usize {
        8
    }

    fn registers() -> ArchitectureRegisters {
        rv64g_registers()
    }

    fn registers_required_for_argument(type_: &midend::types::Type) -> Option<usize> {
        unimplemented!();
        //let size = type_.size::<Self, C>(context).unwrap();
        let size = 123;

        let registers = Self::registers();
        let reg_size = Self::word_size();

        if size <= registers.counts_by_purpose[&RegisterPurpose::Argument] * reg_size {
            Some((size / reg_size) + if size % reg_size != 0 { 1 } else { 0 })
        } else {
            None
        }
    }
}
