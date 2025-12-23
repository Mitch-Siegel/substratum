use std::collections::{BTreeMap, HashSet};

use crate::{midend::*, trace};
pub use errors::*;

mod def_path;
mod errors;
pub mod intrinsics;
pub mod symbols;
pub mod visitor;

pub use def_path::*;
pub use symbols::{types, values, *};
pub use visitor::*;
//pub use symtab_visitor::{MutSymtabVisitor, SymtabVisitor};

pub struct SymbolTable {
    pub types: types::Interner,
    // mapping of symbols to declarations (None) or definitions (Some)
    symbols: BTreeMap<DefPath, Option<SymbolRepr>>,
    children: BTreeMap<DefPath, HashSet<DefPath>>,
}

impl Default for SymbolTable {
    fn default() -> Self {
        Self {
            types: types::Interner::new(),
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

    pub fn declare(&mut self, def_path: DefPath) -> Result<DefPath, SymbolError> {
        let mut parent_def_path = def_path.clone();
        parent_def_path.pop();

        self.children
            .entry(parent_def_path)
            .or_default()
            .insert(def_path.clone());

        match self.symbols.insert(def_path.clone(), None) {
            Some(Some(_already_defined)) => Err(SymbolError::AlreadyDefined(def_path)),
            Some(None) => Err(SymbolError::AlreadyDeclared(def_path)),
            None => Ok(def_path),
        }
    }

    /// define the given symbol at the given path
    /// assumes that the def_path is the full, global DefPath under which S will be inserted
    /// automatically adds the DefPathComponent for S to the end of def_path
    pub fn define<S>(&mut self, def_path: DefPath, symbol: S) -> Result<DefPath, SymbolError>
    where
        S: Symbol + std::fmt::Debug,
    {
        let full_def_path = def_path.clone().with_component(symbol.path_component())?;
        self.children
            .entry(def_path.clone())
            .or_default()
            .insert(full_def_path.clone());

        trace::debug!("insert at {} - {:?}", def_path, symbol.path_component());

        match self
            .symbols
            .insert(full_def_path.clone(), Some(symbol.into_repr()))
        {
            Some(Some(_already_defined)) => Err(SymbolError::AlreadyDefined(def_path)),
            Some(None) | None => Ok(full_def_path),
        }
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

    pub fn defs(&self) -> impl Iterator<Item = (&DefPath, &SymbolRepr)> {
        self.symbols
            .iter()
            .map(|(path, maybe_def)| match maybe_def {
                Some(def) => Some((path, def)),
                None => None,
            })
            .flatten()
    }

    pub fn defs_mut(&mut self) -> impl Iterator<Item = (&DefPath, &mut SymbolRepr)> {
        self.symbols
            .iter_mut()
            .map(|(path, maybe_def)| match maybe_def {
                Some(def) => Some((path, def)),
                None => None,
            })
            .flatten()
    }

    fn lookup_with_path(
        &self,
        def_path: &DefPath,
        subpath: DefPath,
        key: DefPathComponent,
    ) -> Result<(&SymbolRepr, DefPath), SymbolError> {
        let mut search_def_path = def_path.clone();
        let subpath_with_component = subpath.with_component(key.clone())?;

        while !search_def_path.is_empty() {
            let full_path = search_def_path
                .clone()
                .join(subpath_with_component.clone())?;

            match self.symbols.get(&full_path) {
                Some(Some(def)) => {
                    trace::debug!("found key {:?} at defpath {:?}", key, full_path);
                    return Ok((def, full_path));
                }
                Some(None) | None => (),
            }

            search_def_path.pop().unwrap();
        }

        trace::debug!(
            "unable to find key {:?} at defpath {:?} or any of its parents",
            key,
            def_path
        );
        Err(SymbolError::Undefined(def_path.clone(), key))
    }

    fn lookup_mut_with_path(
        &mut self,
        def_path: &DefPath,
        subpath: DefPath,
        key: DefPathComponent,
    ) -> Result<(&mut SymbolRepr, DefPath), SymbolError> {
        let mut search_def_path = def_path.clone();
        let subpath_with_component = subpath.with_component(key.clone())?;

        while !search_def_path.is_empty() {
            let full_path = search_def_path
                .clone()
                .join(subpath_with_component.clone())?;

            match self.symbols.get_mut(&full_path) {
                Some(Some(def)) => {
                    trace::debug!("found key {:?} at defpath {:?}", key, full_path);
                    return Ok((def, full_path));
                }
                Some(None) | None => (),
            }

            search_def_path.pop().unwrap();
        }

        trace::debug!(
            "unable to find key {:?} at defpath {:?} or any of its parents",
            key,
            def_path
        );
        Err(SymbolError::Undefined(def_path.clone(), key))
    }

    pub fn lookup_type(
        &self,
        def_path: &DefPath,
        subpath: DefPath,
        name: String,
    ) -> Result<(&SymbolRepr, DefPath), SymbolError> {
        let (repr, path) =
            self.lookup_with_path(def_path, subpath, DefPathComponent::Type(name))?;
        assert!(matches!(repr, SymbolRepr::Type(_)));
        Ok((repr, path))
    }

    pub fn lookup_value(
        &self,
        def_path: &DefPath,
        subpath: DefPath,
        name: String,
    ) -> Result<(&SymbolRepr, DefPath), SymbolError> {
        let (repr, path) =
            self.lookup_with_path(def_path, subpath, DefPathComponent::Value(name))?;
        assert!(matches!(repr, SymbolRepr::Value(_)));
        Ok((repr, path))
    }
}

/// Type handling helper functions
impl SymbolTable {
    #[tracing::instrument(skip(self), level = "debug")]
    pub fn semantic_type_for_syntactic(
        &self,
        search_def_path: &DefPath,
        generic_params: types::ParamSubstMap,
        ty_: &types::Syntactic,
    ) -> Result<types::Semantic, SymbolError> {
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
                .with_component(DefPathComponent::Module(ModuleName {
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
