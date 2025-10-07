use crate::midend::symtab::*;

#[derive(PartialEq, Eq)]
pub enum SymbolError {
    Undeclared(DefPath, DefPathComponent),
    Undefined(DefPath, DefPathComponent),
    AlreadyDeclared(DefPath),
    AlreadyDefined(DefPath),
    CantOwn(DefPath, DefPathComponent),
}
impl std::fmt::Debug for SymbolError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Undefined(path, component) => write!(
                f,
                "undeclared symbol {:?} at definition path {}",
                component, path
            ),
            Self::Undeclared(path, component) => write!(
                f,
                "undeclared symbol {:?} at definition path {}",
                component, path
            ),
            Self::AlreadyDeclared(path) => write!(f, "DefPath {} is already declared", path),
            Self::AlreadyDefined(path) => write!(f, "DefPath {} is already defined", path),
            Self::CantOwn(owner, ownee) => {
                write!(f, "DefPath \"{:?}\" can't own {:?}", owner, ownee)
            }
        }
    }
}
