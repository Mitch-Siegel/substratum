mod generic;

#[cfg(feature = "arch_RV64G")]
pub mod riscv;
#[cfg(feature = "arch_RV64G")]
pub type Target = riscv::RV64G;
