use crate::midend::symtab::symbols::*;

pub mod binding;
pub mod function;

pub use binding::*;
pub use function::*;

#[enum_delegate::implement(Symbol)]
#[derive(Clone, Debug)]
pub enum Value {
    Function(Function),
    LocalBinding(LocalBinding),
}

impl std::fmt::Display for Value {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Function(fu) => write!(f, "{}", fu),
            Self::LocalBinding(lb) => write!(f, "{}", lb),
        }
    }
}
