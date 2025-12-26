use crate::midend::symtab::{Symbol, *};

pub mod builtins;
pub mod declarations;
pub mod module;

pub use builtins::*;
pub use declarations::*;
pub use module::Module;

#[derive(Debug)]
#[enum_delegate::implement(Symbol)]
pub enum Type {
    Module(Module),
    //UseDeclaration

    //TypeAlias
    BuiltinType(BuiltinType),
    TypeDecl(TypeDecl),
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
            Self::Module(m) => write!(f, "{}", m),
            Self::BuiltinType(bit) => write!(f, "{}", bit),
            Self::TypeDecl(td) => write!(f, "{}", td),
            //Self::GenericTypeParam(_) => write!(f, "generic type param"),
        }
    }
}
