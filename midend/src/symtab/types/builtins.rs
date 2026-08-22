use crate::{
    symtab::{PathSegment, Symbol, SymbolDef, Type},
    types,
};

#[derive(Clone, Debug)]
pub(crate) enum BuiltinType {
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
    pub(crate) fn syntactic(&self) -> types::Syntactic {
        match self {
            Self::Unit => types::Syntactic::Unit,
            Self::U8 => types::Syntactic::U8,
            Self::U16 => types::Syntactic::U16,
            Self::U32 => types::Syntactic::U32,
            Self::U64 => types::Syntactic::U64,
            Self::I8 => types::Syntactic::I8,
            Self::I16 => types::Syntactic::I16,
            Self::I32 => types::Syntactic::I32,
            Self::I64 => types::Syntactic::I64,
        }
    }

    pub(crate) fn generic_params(&self) -> Option<&types::GenericParamsList> {
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

    fn path_segment(&self) -> PathSegment {
        PathSegment::Type(self.name().into())
    }
}

impl From<BuiltinType> for SymbolDef {
    fn from(value: BuiltinType) -> Self {
        Self::Type(Type::Builtin(value))
    }
}

impl std::fmt::Display for BuiltinType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name())
    }
}
