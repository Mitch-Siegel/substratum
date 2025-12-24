use crate::midend::symtab::{Symbol, *};

pub mod declarations;
pub mod module;

pub use declarations::*;
pub use module::Module;

#[derive(Debug)]
pub enum Type {
    Module(Module),
    //UseDeclaration

    //TypeAlias
    TypeDecl(TypeDecl),
    //Union
    //ConstantItem
    //StaticItem
    //Trait
    GenericTypeParam(()),
    //ExternBlock
}

impl Symbol for Type {
    fn name(&self) -> &str {
        match self {
            Self::Module(m) => &m.name,
            Self::TypeDecl(td) => td.name(),
            Self::GenericTypeParam(_) => "generic type params",
        }
    }

    fn into_repr(self) -> SymbolDef {
        SymbolDef::Type(self)
    }

    fn path_segment(&self) -> PathSegment {
        PathSegment::Type(self.name().into())
    }
}

impl std::fmt::Display for Type {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Module(m) => write!(f, "{}", m),
            Self::TypeDecl(td) => write!(f, "{}", td),
            Self::GenericTypeParam(_) => write!(f, "generic type param"),
        }
    }
}
