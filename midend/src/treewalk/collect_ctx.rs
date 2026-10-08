use crate::{
    treewalk::{CollectCtx, CollectResult, PathedCtx, UnpathedCtxTrait, symtab},
    types,
};

pub(crate) struct UnpathedCollectCtx {
    symtab: symtab::SymbolTable,
}

impl UnpathedCollectCtx {
    pub(crate) fn new(symtab: symtab::SymbolTable) -> Self {
        Self { symtab }
    }

    pub(crate) fn take(self) -> symtab::SymbolTable {
        self.symtab
    }
}

impl UnpathedCtxTrait for UnpathedCollectCtx {
    fn with_path<P: symtab::Path>(self, path: P) -> PathedCtx<Self, P> {
        CollectCtx {
            unpathed: self,
            path,
        }
    }

    fn symtab(&self) -> &impl symtab::Symtab {
        &self.symtab
    }

    fn symtab_mut(&mut self) -> &mut impl symtab::Symtab {
        &mut self.symtab
    }

    fn symtab_and_types(&mut self) -> (&mut impl symtab::Symtab, &mut types::Interner) {
        unreachable!();
        #[allow(unreachable_code)] // to appease type checker for 'impl' return
        (&mut symtab::SymbolTable::new(), &mut types::Interner::new())
    }
}

impl<P: symtab::Path> PathedCtx<UnpathedCollectCtx, P> {
    // allow wrap in result for ergonomics returning from linearze
    #[allow(clippy::unnecessary_wraps)]
    pub(crate) fn into_result(self) -> CollectResult {
        Ok(self.unpathed)
    }
}
