use crate::midend::symtab::*;

#[derive(PartialEq, Eq)]
pub enum SymbolError {
    Undefined(DefPath, DefPathComponent),
    Defined(DefPath),
    CantOwn(DefPath, DefPathComponent),
}
impl std::fmt::Debug for SymbolError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Undefined(path, component) => write!(
                f,
                "Undefined symbol {:?} at definition path {}",
                component, path
            ),
            Self::Defined(path) => write!(f, "{}", path),
            Self::CantOwn(owner, ownee) => {
                write!(f, "DefPath \"{:?}\" can't own {:?}", owner, ownee)
            }
        }
    }
}
