use std::collections::{BTreeMap, HashSet};

use crate::{
    midend::{self, *},
    trace,
};
pub use errors::*;

mod def_path;
mod errors;
pub mod intrinsics;
pub mod symbols;
pub mod visitor;

pub mod implementation;
pub mod types;
pub mod values;

pub use def_path::*;
pub use implementation::Implementation;
pub use symbols::*;
pub use types::Type;
pub use values::Value;
pub use visitor::*;

pub trait Symtab {
    fn insert(
        &mut self,
        path: DefPath,
        maybe_symbol: Option<SymbolDef>,
    ) -> Result<DefPath, SymbolError>;

    // declare 'path' to exist
    fn declare(&mut self, path: DefPath) -> Result<DefPath, SymbolError> {
        trace::trace!("declare {}", path);
        self.insert(path, None)
    }

    // define 'symbol' as a child of 'path', returning path::symbol or error
    fn define(&mut self, path: DefPath, symbol: SymbolDef) -> Result<DefPath, SymbolError> {
        match symbol {
            SymbolDef::Type(_) => assert!(path.is_type()),
            SymbolDef::Value(_) => assert!(path.is_value()),
            SymbolDef::Impl(_) => assert!(path.is_impl()),
        }
        assert!(*path.last() == symbol.path_segment());

        match path.clone().without_last() {
            Ok((parent_path, _last_segment)) => {
                if parent_path.is_impl() {
                    if let SymbolDef::Impl(_impl_def) = self.lookup_at(&parent_path)?.unwrap() {
                        // TODO: check that parent_path is a type or module
                        Ok::<(), SymbolError>(())
                    } else {
                        Ok::<(), SymbolError>(())
                    }
                } else {
                    Ok::<(), SymbolError>(())
                }
            }
            Err(PathError::WithoutLastSingleSegment(_)) => Ok(()),
            Err(e) => Err(e.into()),
        }?;

        trace::trace!("define {} at {}", symbol.name(), path);
        self.insert(path, Some(symbol))
    }

    fn lookup_at(&self, path: &DefPath) -> Result<Option<&SymbolDef>, SymbolError>;

    fn lookup_decl_at(&self, path: &DefPath) -> Result<(), SymbolError> {
        match self.lookup_at(path) {
            Ok(_) => Ok(()),
            Err(e) => Err(e),
        }
    }

    fn lookup_def_at(&self, path: &DefPath) -> Result<&SymbolDef, SymbolError> {
        match self.lookup_at(path) {
            Ok(Some(symbol)) => Ok(symbol),
            Ok(None) => Err(SymbolError::Undefined(path.clone())),
            Err(e) => Err(e),
        }
    }

    fn lookup_def(
        &self,
        search_path: DefPath,
        lookup_path: DefPath,
    ) -> Result<(&SymbolDef, DefPath), SymbolError> {
        let mut search_segments = search_path.clone().into_iter().collect::<Vec<_>>();
        let lookup_segments = lookup_path.clone().into_iter().collect::<Vec<_>>();
        let (symbol_segment, lookup_segments) = lookup_segments.split_last().unwrap();
        while !search_segments.is_empty() {
            let all_prefix_segments = search_path
                .clone()
                .into_iter()
                .chain(lookup_segments.to_owned())
                .collect::<Vec<_>>();
            let search_path = DefPath::new(all_prefix_segments, symbol_segment.to_owned());
            match self.lookup_at(&search_path) {
                Ok(Some(symbol)) => {
                    return Ok((symbol, search_path));
                }
                Ok(None) | Err(_) => {
                    search_segments.pop().unwrap();
                }
            }
        }

        Err(SymbolError::Undeclared(lookup_path))
    }

    fn lookup_decl(
        &self,
        search_path: DefPath,
        lookup_path: DefPath,
    ) -> Result<DefPath, SymbolError> {
        let mut search_segments = search_path.clone().into_iter().collect::<Vec<_>>();
        let lookup_segments = lookup_path.clone().into_iter().collect::<Vec<_>>();
        let (symbol_segment, lookup_segments) = lookup_segments.split_last().unwrap();
        while !search_segments.is_empty() {
            let all_prefix_segments = search_path
                .clone()
                .into_iter()
                .chain(lookup_segments.to_owned())
                .collect::<Vec<_>>();
            let search_path = DefPath::new(all_prefix_segments, symbol_segment.to_owned());
            match self.lookup_at(&search_path) {
                Ok(Some(_)) | Ok(None) => {
                    return Ok(search_path);
                }
                Err(_) => {
                    search_segments.pop().unwrap();
                }
            }
        }

        Err(SymbolError::Undeclared(lookup_path))
    }

    fn get_impls_for(&self, path: &DefPath) -> Result<&HashSet<DefPath>, SymbolError>;

    fn create_impl(
        &mut self,
        impl_parent_path: DefPath,
        impl_for_path: DefPath,
    ) -> Result<DefPath, SymbolError>;
}

pub struct SymbolTable {
    pub types: midend::types::Interner,
    // mapping of symbols to declarations (None) or definitions (Some)
    symbols: BTreeMap<DefPath, Option<SymbolDef>>,
    children: BTreeMap<DefPath, HashSet<DefPath>>,
    impls: BTreeMap<DefPath, HashSet<DefPath>>,
}

impl Default for SymbolTable {
    fn default() -> Self {
        Self {
            types: midend::types::Interner::new(),
            symbols: BTreeMap::new(),
            children: BTreeMap::new(),
            impls: BTreeMap::new(),
        }
    }
}

impl std::fmt::Debug for SymbolTable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        //writeln!(f, "types: {:?}", self.types)?;
        writeln!(f, "definitions:")?;

        for (path, def) in &self.symbols {
            match def {
                Some(def) => writeln!(f, "defpath {} - {:?}", path, def,)?,
                _ => writeln!(f, "defpath {} - {:?}", path, def)?,
            }
        }
        Ok(())
    }
}

impl SymbolTable {
    pub fn new() -> Self {
        let mut symtab = Self::default();
        intrinsics::create_core(&mut symtab);

        symtab
    }

    pub fn children(&self, def_path: &DefPath) -> HashSet<&DefPath> {
        match self.children.get(def_path) {
            Some(paths) => paths.iter().collect(),
            None => HashSet::new(),
        }
    }

    pub fn decls(&self) -> impl Iterator<Item = &DefPath> {
        self.symbols.keys()
    }

    pub fn defs(&self) -> impl Iterator<Item = (&DefPath, &SymbolDef)> {
        self.symbols
            .iter()
            .filter_map(|(path, maybe_def)| maybe_def.as_ref().map(|def| (path, def)))
    }

    pub fn defs_mut(&mut self) -> impl Iterator<Item = (&DefPath, &mut SymbolDef)> {
        self.symbols
            .iter_mut()
            .filter_map(|(path, maybe_def)| maybe_def.as_mut().map(|def| (path, def)))
    }

    pub fn define_type<S>(
        &mut self,
        parent_path: DefPath,
        symbol: S,
    ) -> Result<DefPath, SymbolError>
    where
        S: Into<Type>,
    {
        self.define(parent_path, symbol.into().into())
    }

    pub fn define_value(
        &mut self,
        parent_path: DefPath,
        symbol: Value,
    ) -> Result<DefPath, SymbolError> {
        self.define(parent_path, SymbolDef::Value(symbol))
    }
}

impl Symtab for SymbolTable {
    fn insert(
        &mut self,
        path: DefPath,
        maybe_symbol: Option<SymbolDef>,
    ) -> Result<DefPath, SymbolError> {
        let allow_definition = maybe_symbol.is_some();
        if path.len() > 1 {
            let (parent_path, _) = path.clone().without_last().unwrap();
            if !self
                .children
                .entry(parent_path)
                .or_default()
                .insert(path.clone())
                && !allow_definition
            {
                panic!("untracked child path {}", path)
            }
        }

        match self.symbols.insert(path.clone(), maybe_symbol) {
            Some(Some(_)) => Err(SymbolError::AlreadyDefined(path)),
            Some(None) => {
                if allow_definition {
                    Ok(path)
                } else {
                    Err(SymbolError::AlreadyDeclared(path))
                }
            }
            None => Ok(path),
        }
    }

    fn lookup_at(&self, path: &DefPath) -> Result<Option<&SymbolDef>, SymbolError> {
        match self.symbols.get(path) {
            Some(maybe_symbol) => Ok(maybe_symbol.as_ref()),
            None => Err(SymbolError::Undeclared(path.clone())),
        }
    }

    fn get_impls_for(&self, path: &DefPath) -> Result<&HashSet<DefPath>, SymbolError> {
        self.impls
            .get(path)
            .ok_or(SymbolError::Undeclared(path.clone()))
    }

    fn create_impl(
        &mut self,
        _impl_parent_path: DefPath,
        _impl_for_path: DefPath,
    ) -> Result<DefPath, SymbolError> {
        unimplemented!();
    }
}

/// Type handling helper functions
impl SymbolTable {
    #[tracing::instrument(skip(self), level = "debug")]
    pub fn semantic_type_for_syntactic(
        &self,
        search_def_path: &DefPath,
        generic_params: midend::types::ParamSubstMap,
        ty_: &midend::types::Syntactic,
    ) -> Result<midend::types::Semantic, SymbolError> {
        unimplemented!();
        /*
        let (_, path) = self.lookup_type(search_def_path, ty_)?;
        trace::trace!(
            "found definition of syntactic type {} at defpath {}",
            ty_,
            path
        );
        Ok(self
            .types
            .semantic_for_defpath(path, generic_params)
            .unwrap())
        */
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn insert_and_lookup() {
        let mut symtab = SymbolTable::new();

        assert_eq!(
            symtab.define(DefPath::empty(), Module::new("test_mod".into())),
            Ok(DefPath::empty()
                .with_component(PathSegment::Module(ModuleName {
                    name: "test_mod".into()
                }))
                .unwrap())
        );

        /*
        assert_eq!(
            symtab.insert(DefPath::new(), Module::new("test_mod".into())),
            Err(SymbolError::Defined(DefPath::new().with_component(
                DefPathComponent::Module(ModuleName("test_mod".into()))
            )))
        );
        */
    }
}
