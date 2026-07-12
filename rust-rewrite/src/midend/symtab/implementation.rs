use crate::midend::{symtab::*, types};

#[derive(Debug)]
pub(crate) struct Implementation {
    pub id: ImplId,
    // FUTURE: generics
    pub self_ty: types::Syntactic,
    pub self_ty_path_link: Option<TypePath>,
    // FUTURE: trait information
}

impl Implementation {
    pub(crate) fn new(id: ImplId, self_ty: types::Syntactic) -> Self {
        Self {
            id,
            self_ty,
            self_ty_path_link: None,
        }
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
