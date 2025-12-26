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

pub use def_path::*;
pub use symbols::*;
pub use visitor::*;

pub trait Symtab {
    fn insert(
        &mut self,
        path: DefPath,
        maybe_symbol: Option<SymbolDef>,
    ) -> Result<DefPath, SymbolError>;

    // declare 'path' to exist
    fn declare(&mut self, path: DefPath) -> Result<DefPath, SymbolError> {
        trace::debug!("declare {}", path);
        self.insert(path, None)
    }

    // define 'symbol' as a child of 'path', returning path::symbol or error
    fn define(&mut self, parent_path: DefPath, symbol: SymbolDef) -> Result<DefPath, SymbolError> {
        match symbol {
            SymbolDef::Type(_) => assert!(parent_path.is_type()),
            SymbolDef::Value(_) => assert!(parent_path.is_value()),
        }

        trace::debug!("define {} at {}", symbol.name(), parent_path);
        self.insert(
            parent_path.with_segment(symbol.path_segment())?,
            Some(symbol),
        )
    }

    fn lookup(
        &self,
        search_path: DefPath,
        lookup_path: DefPath,
    ) -> Result<(&SymbolDef, DefPath), SymbolError>;
}

pub struct SymbolTable {
    pub types: midend::types::Interner,
    // mapping of symbols to declarations (None) or definitions (Some)
    symbols: BTreeMap<DefPath, Option<SymbolDef>>,
    children: BTreeMap<DefPath, HashSet<DefPath>>,
}

impl Default for SymbolTable {
    fn default() -> Self {
        Self {
            types: midend::types::Interner::new(),
            symbols: BTreeMap::new(),
            children: BTreeMap::new(),
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
            Some(paths) => paths.iter().map(|path_ref| path_ref).collect(),
            None => HashSet::new(),
        }
    }

    pub fn decls(&self) -> impl Iterator<Item = &DefPath> {
        self.symbols.iter().map(|(path, _)| path)
    }

    pub fn defs(&self) -> impl Iterator<Item = (&DefPath, &SymbolDef)> {
        self.symbols
            .iter()
            .map(|(path, maybe_def)| match maybe_def {
                Some(def) => Some((path, def)),
                None => None,
            })
            .flatten()
    }

    pub fn defs_mut(&mut self) -> impl Iterator<Item = (&DefPath, &mut SymbolDef)> {
        self.symbols
            .iter_mut()
            .map(|(path, maybe_def)| match maybe_def {
                Some(def) => Some((path, def)),
                None => None,
            })
            .flatten()
    }

    pub fn define_type(
        &mut self,
        parent_path: DefPath,
        symbol: Type,
    ) -> Result<DefPath, SymbolError> {
        self.define(parent_path, SymbolDef::Type(symbol))
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
        if path.len() > 1 {
            let (parent_path, _) = path.clone().without_last().unwrap();
            if !self
                .children
                .entry(parent_path.into())
                .or_default()
                .insert(path.clone())
            {
                panic!("untracked child path {}", path)
            }
        }

        match self.symbols.insert(path.clone(), maybe_symbol) {
            Some(Some(_)) => Err(SymbolError::AlreadyDefined(path)),
            Some(None) => Err(SymbolError::AlreadyDeclared(path)),
            None => Ok(path),
        }
    }

    fn lookup(
        &self,
        mut search_path: DefPath,
        lookup_path: DefPath,
    ) -> Result<(&SymbolDef, DefPath), SymbolError> {
        while search_path.len() > 0 {
            match search_path.clone().join(lookup_path.clone()) {
                Ok(full_path) => match self.symbols.get(&full_path) {
                    Some(Some(symbol)) => return Ok((symbol, full_path)),
                    _ => (),
                },
                Err(_) => (),
            }
            search_path = search_path.without_last().unwrap().0;
        }
        Err(SymbolError::Undefined(lookup_path))
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
