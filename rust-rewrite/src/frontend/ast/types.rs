use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct TypeTree {
    pub loc: SourceLoc,
    pub type_: midend::types::Type,
}

impl TypeTree {
    pub fn new(loc: SourceLoc, type_: midend::types::Type) -> Self {
        Self { loc, type_ }
    }
}

impl Display for TypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.type_)
    }
}

impl ReturnWalk<midend::types::Type> for TypeTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut impl midend::linearizer::DefContext) -> midend::types::Type {
        // TODO: check that the type exists by looking it up
        match self.type_ {
            midend::types::Type::Unit
            | midend::types::Type::U8
            | midend::types::Type::U16
            | midend::types::Type::U32
            | midend::types::Type::U64
            | midend::types::Type::I8
            | midend::types::Type::I16
            | midend::types::Type::I32
            | midend::types::Type::I64 => self.type_,
            midend::types::Type::_Self => self.type_,
            midend::types::Type::Named(name) => context.resolve_type_name(name.as_str()).unwrap(),
            // TODO: resolve reference/pointer correctly
            midend::types::Type::Reference(_, _) | midend::types::Type::Pointer(_, _) => self.type_,
            _ => panic!("Unexpected type seen in type tree: {}", self.type_),
        }
    }
}
