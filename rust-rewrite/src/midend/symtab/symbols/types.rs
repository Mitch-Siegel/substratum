use crate::midend::symtab::{Symbol, *};

pub mod enumeration;
pub mod implementation;
pub mod module;
pub mod structure;

pub use enumeration::{EnumRepr, EnumVariantRepr};
pub use implementation::ImplRepr;
pub use module::Module;
pub use structure::StructRepr;

#[derive(Debug)]
pub enum Type {
    Module(Module),
    //UseDeclaration

    //TypeAlias
    Struct(StructRepr),
    Enum(EnumRepr),
    //Union
    //ConstantItem
    //StaticItem
    //Trait
    Implementation(ImplRepr),
    GenericTypeParam(()),
    //ExternBlock
}

impl Symbol for Type {
    fn name(&self) -> &str {
        match self {
            Self::Module(m) => &m.name,
            Self::Struct(s) => &s.name,
            Self::Enum(e) => &e.name,
            Self::Implementation(i) => i.name(),
            Self::GenericTypeParam(_) => "generic type params",
        }
    }

    fn into_repr(self) -> SymbolRepr {
        SymbolRepr::Type(self)
    }

    fn path_component(&self) -> DefPathComponent {
        DefPathComponent::Type(self.name().into())
    }
}
