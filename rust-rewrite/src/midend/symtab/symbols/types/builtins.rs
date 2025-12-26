use crate::midend::{symtab::*, *};

#[derive(Clone, Debug)]
pub enum BuiltinType {
    Unit,
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
}

impl BuiltinType {
    pub fn syntactic(&self) -> midend::types::Syntactic {
        match self {
            Self::Unit => midend::types::Syntactic::Unit,
            Self::U8 => midend::types::Syntactic::U8,
            Self::U16 => midend::types::Syntactic::U16,
            Self::U32 => midend::types::Syntactic::U32,
            Self::U64 => midend::types::Syntactic::U64,
            Self::I8 => midend::types::Syntactic::I8,
            Self::I16 => midend::types::Syntactic::I16,
            Self::I32 => midend::types::Syntactic::I32,
            Self::I64 => midend::types::Syntactic::I64,
        }
    }

    pub fn generic_params(&self) -> Option<&midend::types::GenericParamsList> {
        None
    }
}

impl Symbol for BuiltinType {
    fn name(&self) -> &str {
        match self {
            Self::Unit => "()",
            Self::U8 => "u8",
            Self::U16 => "u16",
            Self::U32 => "u32",
            Self::U64 => "u64",
            Self::I8 => "i8",
            Self::I16 => "i16",
            Self::I32 => "i32",
            Self::I64 => "i64",
        }
    }

    fn into_repr(self) -> symtab::SymbolDef {
        symtab::Type::BuiltinType(self).into_repr()
    }

    fn path_segment(&self) -> symtab::PathSegment {
        PathSegment::Type(self.name().into())
    }
}

impl std::fmt::Display for BuiltinType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name())
    }
}
