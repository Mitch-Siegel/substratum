use crate::midend::symtab::{midend, symtab, PathSegment, Symbol};

pub(crate) mod enumeration;
pub(crate) mod structure;

pub(crate) use enumeration::*;
pub(crate) use structure::*;

#[derive(Clone, Debug)]
pub(crate) enum DeclaredType {
    Struct(StructRepr),
    Enum(EnumRepr),
}

impl From<StructRepr> for DeclaredType {
    fn from(value: StructRepr) -> Self {
        Self::Struct(value)
    }
}

impl From<EnumRepr> for DeclaredType {
    fn from(value: EnumRepr) -> Self {
        Self::Enum(value)
    }
}

impl std::fmt::Display for DeclaredType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Struct(s) => write!(f, "{s}"),
            Self::Enum(e) => write!(f, "{e}"),
        }
    }
}

#[derive(Clone, Debug)]
pub(crate) struct TypeDecl {
    pub declared_type: DeclaredType,
    pub generic_params: midend::types::GenericParamsList,
}

impl TypeDecl {
    pub(crate) fn syntactic(&self) -> midend::types::Syntactic {
        match &self.declared_type {
            DeclaredType::Struct(s) => midend::types::Syntactic::Named(s.name.clone()),
            DeclaredType::Enum(e) => midend::types::Syntactic::Named(e.name.clone()),
        }
    }

    pub(crate) fn generic_params(&self) -> &midend::types::GenericParamsList {
        &self.generic_params
    }
}

impl Symbol for TypeDecl {
    fn name(&self) -> &str {
        match &self.declared_type {
            DeclaredType::Struct(s) => &s.name,
            DeclaredType::Enum(e) => &e.name,
        }
    }

    fn path_segment(&self) -> symtab::PathSegment {
        PathSegment::Type(self.name().into())
    }
}

impl From<TypeDecl> for symtab::SymbolDef {
    fn from(value: TypeDecl) -> Self {
        Self::from(symtab::Type::Decl(value))
    }
}

impl std::fmt::Display for TypeDecl {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.declared_type)?;
        if self.generic_params.is_empty() {
            Ok(())
        } else {
            write!(f, "<")?;
            let mut first = true;
            for p in &self.generic_params {
                if first {
                    write!(f, "{p}")?;
                    first = false;
                } else {
                    write!(f, ", {p}")?;
                }
            }
            write!(f, ">")
        }
    }
}
