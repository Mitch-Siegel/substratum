use crate::midend::symtab::{PathSegment, Symbol, SymbolDef};

pub(crate) mod builtins;
pub(crate) mod declarations;
pub(crate) mod module;

pub(crate) use builtins::*;
pub(crate) use declarations::*;
pub(crate) use module::Module;

#[derive(Debug)]
#[enum_delegate::implement(Symbol)]
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
