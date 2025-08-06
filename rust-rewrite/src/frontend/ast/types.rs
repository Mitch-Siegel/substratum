use crate::frontend::ast::*;

#[derive(ReflectName, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct TypeTree {
    pub loc: SourceLoc,
    pub type_: midend::types::Syntactic,
}

impl TypeTree {
    pub fn new(loc: SourceLoc, type_: midend::types::Syntactic) -> Self {
        Self { loc, type_ }
    }
}

impl Display for TypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.type_)
    }
}

impl std::fmt::Debug for TypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", serde_json::to_string(self).unwrap())
    }
}

impl ReturnWalk<midend::types::Syntactic> for TypeTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut impl midend::linearizer::DefContext) -> midend::types::Syntactic {
        // TODO: check that the type exists by looking it up
        match self.type_ {
            midend::types::Syntactic::Unit
            | midend::types::Syntactic::U8
            | midend::types::Syntactic::U16
            | midend::types::Syntactic::U32
            | midend::types::Syntactic::U64
            | midend::types::Syntactic::I8
            | midend::types::Syntactic::I16
            | midend::types::Syntactic::I32
            | midend::types::Syntactic::I64 => self.type_,
            midend::types::Syntactic::_Self => self.type_,
            midend::types::Syntactic::Named(name) => {
                context.resolve_type_name(name.as_str()).unwrap()
            }
            // TODO: resolve reference/pointer correctly
            midend::types::Syntactic::Reference(_, _) | midend::types::Syntactic::Pointer(_, _) => {
                self.type_
            }
            _ => panic!("Unexpected type seen in type tree: {}", self.type_),
        }
    }
}
