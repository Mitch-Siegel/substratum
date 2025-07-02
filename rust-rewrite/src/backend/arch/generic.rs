pub mod registers;
use crate::midend;
pub use registers::*;

pub trait TargetArchitecture {
    // returns the size of the machine word in bytes
    fn word_size() -> usize;

    // returns the registers available in the architecture and their functions
    fn registers() -> ArchitectureRegisters;

    // implements rules around which arguments may be placed in registers
    // returns:
    // - None if argument type may not be placed in a register
    // - Some(n) where n is the number of argument registers required if the type may be
    // placed in an argument register
    fn registers_required_for_argument(type_: &midend::types::Type) -> Option<usize>;
}
