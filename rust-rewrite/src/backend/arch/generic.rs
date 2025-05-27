pub mod registers;
pub use registers::RegisterPurpose;
pub use registers::*;

pub trait TargetArchitecture {
    fn registers() -> ArchitectureRegisters;
}
