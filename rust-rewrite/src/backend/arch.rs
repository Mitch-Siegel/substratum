pub(crate) mod generic;

#[cfg(feature = "arch_RV64G")]
pub(crate) mod riscv;
#[cfg(feature = "arch_RV64G")]
pub(crate) type Target = riscv::RV64G;
