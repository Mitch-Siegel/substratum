pub mod generic;
pub use generic::TargetArchitecture;

#[cfg(feature = "arch_RV64G")]
pub mod riscv;
#[cfg(feature = "arch_RV64G")]
pub type Target = riscv::RV64G;
#[cfg(feature = "arch_RV64G")]
pub use riscv::*;
