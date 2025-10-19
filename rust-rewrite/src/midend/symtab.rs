use std::collections::{BTreeMap, HashSet};

use crate::{midend::*, trace};
pub use errors::*;

mod def_path;
mod errors;
pub mod intrinsics;
pub mod symbol;
pub mod visitor;

pub use def_path::*;
pub use symbol::*;
pub use visitor::*;
//pub use symtab_visitor::{MutSymtabVisitor, SymtabVisitor};

pub struct SymbolTable {
    pub types: types::Interner,
    // mapping of symbols to declarations (None) or definitions (Some)
    symbols: BTreeMap<DefPath, Option<SymbolDef>>,
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
                Some(SymbolDef::Type(type_id)) => writeln!(
                    f,
                    "defpath {} - {:?} ({:?})",
                    path,
                    def,
                    self.types.get_type_definition(type_id).unwrap()
                )?,
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

    pub fn lookup_decl(
        &self,
        def_path: &DefPath,
        key_component: &DefPathComponent,
    ) -> Result<DefPath, SymbolError> {
        let paths_to_search = self.build_search_path_from_def_path(def_path);

        for path in paths_to_search {
            let component_def_path = path.clone().with_component(key_component.clone()).unwrap();

            match self.symbols.get(&component_def_path) {
                Some(Some(_)) | Some(None) => {
                    return Ok(component_def_path);
                }
                None => (),
            }
        }

        Err(SymbolError::Undefined(
            def_path.clone(),
            key_component.clone(),
        ))
    }

    pub fn lookup_decl_at(&self, def_path: &DefPath) -> Result<(), SymbolError> {
        match self.symbols.get(&def_path) {
            Some(_symbol) => Ok(()),
            None => {
                let mut owned_def_path = def_path.clone();
                let last = owned_def_path.pop().unwrap();
                Err(SymbolError::Undeclared(owned_def_path, last))
            }
        }
    }

    /// define the given symbol at the given path
    /// assumes that the def_path is the full, global DefPath under which S will be inserted
    /// automatically adds the DefPathComponent for S to the end of def_path
    pub fn define<S>(&mut self, def_path: DefPath, symbol: S) -> Result<DefPath, SymbolError>
    where
        S: Symbol + std::fmt::Debug,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a, 'b> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let full_def_path = def_path
            .clone()
            .with_component((symbol.symbol_key()).clone().into())?;
        self.children
            .entry(def_path.clone())
            .or_default()
            .insert(full_def_path.clone());

        trace::debug!("insert at {} - {:?}", def_path, symbol);

        let symbol = Into::<SymbolDef>::into(DefGenerator::new(
            full_def_path.clone(),
            &mut self.types,
            symbol,
        ));

        match self.symbols.insert(full_def_path.clone(), Some(symbol)) {
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

    // TODO: build this smarter so it can lazily evaluate
    fn build_search_path_from_def_path<'a>(&'a self, def_path: &DefPath) -> Vec<&'a DefPath> {
        let mut search_paths = Vec::new();
        let mut search_def_path = def_path.clone();

        while !search_def_path.is_empty() {
            let old_search_path = search_def_path.clone();
            let _ = search_def_path.pop().unwrap();

            // get a reference to the true instance of the old search def path (owned by the symtab
            // itself)
            let search_path_ref = self
                .children
                .get(&search_def_path)
                .unwrap()
                .get(&old_search_path)
                .unwrap();
            search_paths.push(search_path_ref);

            // get the children of the current search def path (they must exist, we just popped
            // from a child path to make search_def_path the parent of where we just were)
            let search_children = self.children.get(&search_def_path).unwrap();

            // if there are any imports, we need to record them
            for child in search_children {
                if let DefPathComponent::Import(_) = child.last() {
                    if let Some(SymbolDef::Import(import)) = self.symbols.get(child).unwrap() {
                        search_paths.push(&import.qualified_path);
                    }
                }
            }
        }

        search_paths
    }

    /// perform a scoped lookup of key at def_path, checking def_path and all its parents for key
    /// returns a reference to the symbol
    pub fn lookup<S>(
        &self,
        def_path: &DefPath,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        match self.lookup_with_path(def_path, key) {
            Ok((symbol, _)) => Ok(symbol),
            Err(e) => Err(e),
        }
    }

    /// perform a scoped lookup of key at def_path, checking def_path and all its parents for key
    /// returns a reference to the symbol alongside the defpath at which the symbol was found
    pub fn lookup_with_path<S>(
        &self,
        def_path: &DefPath,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<(&S, DefPath), SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let paths_to_search = self.build_search_path_from_def_path(def_path);
        let key_component = Into::<DefPathComponent>::into(key.clone());

        for path in paths_to_search {
            if path.can_own(&key_component) {
                let component_def_path =
                    path.clone().with_component(key_component.clone()).unwrap();

                match self.symbols.get(&component_def_path) {
                    Some(Some(def)) => {
                        let resolver = DefResolver::new(&self.types, def);
                        return Ok((<&S>::from(resolver), component_def_path));
                    }
                    Some(None) | None => (),
                }
            }
        }

        Err(SymbolError::Undefined(def_path.clone(), key_component))
    }

    /// perform a scoped lookup of key at def_path, checking def_path and all its parents for key
    /// returns a mutable reference to the symbol
    pub fn lookup_mut<S>(
        &mut self,
        def_path: &DefPath,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<&mut S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let mut scan_def_path = def_path.clone();
        let key_component = Into::<DefPathComponent>::into(key.clone());

        let defs_ptr = &mut self.symbols as *mut BTreeMap<DefPath, Option<SymbolDef>>;
        let types_ptr = &mut self.types as *mut types::Interner;

        while !scan_def_path.is_empty() {
            if scan_def_path.can_own(&key_component) {
                let component_def_path = scan_def_path
                    .clone()
                    .with_component(key_component.clone())
                    .unwrap();

                unsafe {
                    let defs = &mut *defs_ptr;
                    if let Some(Some(symbol)) = defs.get_mut(&component_def_path) {
                        let types = &mut *types_ptr;

                        let resolver = MutDefResolver::new(types, symbol);
                        return Ok(<&mut S>::from(resolver));
                    }
                }
            }
            scan_def_path.pop();
        }

        Err(SymbolError::Undefined(def_path.clone(), key_component))
    }

    /// perform a lookup at the exact path specified, returning a reference to the symbol
    pub fn lookup_at<S>(&self, def_path: &DefPath) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        match self.symbols.get(&def_path) {
            Some(Some(def)) => {
                let resolver = DefResolver::new(&self.types, def);
                return Ok(<&S>::from(resolver));
            }
            Some(None) => Err(SymbolError::Undeclared(
                def_path.clone(),
                def_path.last().clone(),
            )),
            None => Err(SymbolError::Undefined(
                def_path.clone(),
                def_path.last().clone(),
            )),
        }
    }

    /// perform a lookup at the exact path specified, returning a mutable reference to the symbol
    pub fn lookup_at_mut<S>(&mut self, def_path: &DefPath) -> Result<&mut S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        match self.symbols.get_mut(&def_path) {
            Some(Some(def)) => {
                let resolver = MutDefResolver::new(&mut self.types, def);
                return Ok(<&mut S>::from(resolver));
            }
            Some(None) => Err(SymbolError::Undeclared(
                def_path.clone(),
                def_path.last().clone(),
            )),
            None => Err(SymbolError::Undefined(
                def_path.clone(),
                def_path.last().clone(),
            )),
        }
    }

    pub fn lookup_under<S>(
        &mut self,
        def_path: &DefPath,
        child_path: DefPath,
    ) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let mut scan_parent_path = def_path.clone();
        loop {
            if scan_parent_path.can_own(child_path.first()) {
                let scan_def_path = scan_parent_path.clone().join(child_path.clone()).unwrap();

                match self.lookup_at::<S>(&scan_def_path) {
                    Ok(symbol) => return Ok(symbol),
                    Err(_) => (),
                }
            }

            if scan_parent_path.is_empty() {
                break;
            }
            scan_parent_path.pop().unwrap();
        }

        Err(SymbolError::Undefined(
            child_path.clone(),
            child_path.last().clone(),
        ))
    }
}

/// Type handling helper functions
impl SymbolTable {
    pub fn semantic_type_for_syntactic(
        &self,
        search_def_path: &DefPath,
        ty_: &types::Syntactic,
    ) -> Result<types::Semantic, SymbolError> {
        let (_, path) = self.lookup_with_path::<TypeDefinition>(search_def_path, ty_)?;
        Ok(self.types.semantic_for_defpath(&path).unwrap())
    }
}

/// Post symbol-collection implementation linking
impl SymbolTable {
    pub fn collect_impls(&mut self) {
        let _impls: HashSet<(&DefPath, &ImplementationName)> = self
            .symbols
            .iter()
            .map(|(path, _)| match path.last() {
                DefPathComponent::Implementation(impl_name) => Some((path, impl_name)),
                _ => None,
            })
            .flatten()
            .collect();
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
