use crate::midend::{symtab::*, types::Type};

#[derive(PartialEq, Eq)]
pub enum SymbolError<'a> {
    Undefined(DefPath<'a>),
    Defined(DefPath<'a>),
    CantOwn(DefPathComponent<'a>, DefPathComponent<'a>),
}
impl<'a> std::fmt::Debug for SymbolError<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Undefined(path) => write!(f, "{}", path),
            Self::Defined(path) => write!(f, "{}", path),
            Self::CantOwn(owner, ownee) => write!(f, "{} can't own {}", owner, ownee),
        }
    }
}
