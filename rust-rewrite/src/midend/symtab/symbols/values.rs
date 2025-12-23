use crate::midend::symtab::symbols::*;

pub mod binding;
pub mod function;

pub use binding::LocalBinding;
pub use function::Function;

#[enum_delegate::implement(Symbol)]
pub enum Value {
    Function(Function),
    LocalBinding(LocalBinding),
}
