use crate::midend::treewalk::*;

use std::collections::HashSet;

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
}

impl<P: symtab::Path> PathedCtx<UnpathedCollectCtx, P> {
    pub(crate) fn into_result(self) -> CollectResult {
        Ok(self.unpathed)
    }
}

impl symtab::SymtabBase for UnpathedCollectCtx {
    fn insert(
        &mut self,
        path: symtab::RawPath,
        maybe_symbol: Option<symtab::SymbolDef>,
    ) -> Result<symtab::RawPath, symtab::SymbolError> {
        self.symtab.insert(path, maybe_symbol)
    }

    fn lookup_at(
        &self,
        path: &symtab::RawPath,
    ) -> Result<Option<&symtab::SymbolDef>, symtab::SymbolError> {
        self.symtab.lookup_at(path)
    }

    fn lookup_at_mut(
        &mut self,
        path: &symtab::RawPath,
    ) -> Result<Option<&mut symtab::SymbolDef>, symtab::SymbolError> {
        self.symtab.lookup_at_mut(path)
    }
}

impl symtab::Symtab for UnpathedCollectCtx {
    fn get_impls_for(
        &self,
        path: &symtab::TypePath,
    ) -> Result<&HashSet<symtab::ImplPath>, symtab::SymbolError> {
        self.symtab.get_impls_for(path)
    }

    fn create_impl(
        &mut self,
        impl_parent_path: symtab::RawPath,
        impl_for_path: symtab::TypePath,
    ) -> Result<symtab::ImplPath, symtab::SymbolError> {
        self.symtab.create_impl(impl_parent_path, impl_for_path)
    }

    fn semantic_type_for_syntactic(
        &self,
        search_def_path: &impl symtab::Path,
        generic_params: crate::midend::types::ParamSubstMap,
        ty_: &crate::midend::types::Syntactic,
    ) -> Result<crate::midend::types::Semantic, symtab::SymbolError> {
        self.symtab
            .semantic_type_for_syntactic(search_def_path, generic_params, ty_)
    }
}
