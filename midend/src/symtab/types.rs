use crate::symtab::{PathSegment, Symbol, SymbolDef};

pub(crate) mod builtins;
pub(crate) mod declarations;
pub(crate) mod module;

pub(crate) use builtins::*;
pub(crate) use declarations::*;
pub(crate) use module::Module;

#[derive(Debug)]
pub(crate) enum Type {
    Module(Module),
    //UseDeclaration

    //TypeAlias
    Builtin(BuiltinType),
    Decl(TypeDecl),
    //Union
    //ConstantItem
    //StaticItem
    //Trait
    //GenericTypeParam(()),
    //ExternBlock
}

impl Symbol for Type {
    fn name(&self) -> &str {
        match self {
            Self::Module(m) => m.name(),
            Self::Builtin(b) => b.name(),
            Self::Decl(d) => d.name(),
        }
    }

    fn path_segment(&self) -> PathSegment {
        match self {
            Self::Module(m) => m.path_segment(),
            Self::Builtin(b) => b.path_segment(),
            Self::Decl(d) => d.path_segment(),
        }
    }
}

impl From<Type> for SymbolDef {
    fn from(value: Type) -> Self {
        match value {
            Type::Module(m) => Self::from(m),
            Type::Builtin(b) => Self::from(b),
            Type::Decl(d) => Self::from(d),
        }
    }
}

impl std::fmt::Display for Type {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Module(m) => write!(f, "{m}"),
            Self::Builtin(bit) => write!(f, "{bit}"),
            Self::Decl(td) => write!(f, "{td}"),
            //Self::GenericTypeParam(_) => write!(f, "generic type param"),
        }
    }
}
