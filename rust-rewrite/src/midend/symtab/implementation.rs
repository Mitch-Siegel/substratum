use crate::midend::{symtab::*, types};

#[derive(Debug)]
pub struct Implementation {
    pub id: ImplId,
    // FUTURE: generics
    pub self_ty: types::Syntactic,
    // FUTURE: trait information
}

impl Implementation {
    pub fn new(id: ImplId, self_ty: types::Syntactic) -> Self {
        Self { id, self_ty }
    }
}

impl Symbol for Implementation {
    fn name(&self) -> &str {
        "implementation"
    }

    fn path_segment(&self) -> def_path::PathSegment {
        def_path::PathSegment::Impl(self.id)
    }

    fn into_repr(self) -> SymbolDef {
        SymbolDef::Impl(self)
    }
}
