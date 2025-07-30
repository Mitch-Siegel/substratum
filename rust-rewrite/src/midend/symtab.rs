use std::collections::{BTreeMap, HashSet};

use crate::{midend::ir, trace};
pub use errors::*;

mod def_path;
mod errors;
pub mod intrinsics;
pub mod symbol;
pub mod symtab_visitor;
pub mod type_interner;

pub use def_path::*;
pub use symbol::*;
//pub use symtab_visitor::{MutSymtabVisitor, SymtabVisitor};
pub use type_interner::*;

pub struct SymbolTable {
    types: TypeInterner,
    defs: BTreeMap<DefPath, SymbolDef>,
    children: BTreeMap<DefPath, HashSet<DefPath>>,
}

impl Default for SymbolTable {
    fn default() -> Self {
        Self {
            types: TypeInterner::new(),
            defs: BTreeMap::new(),
            children: BTreeMap::new(),
        }
    }
}

impl std::fmt::Debug for SymbolTable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        //writeln!(f, "types: {:?}", self.types)?;
        writeln!(f, "definitions:")?;

        for (path, def) in &self.defs {
            match def {
                SymbolDef::Type(type_id) => writeln!(
                    f,
                    "defpath {} - {:?} ({:?})",
                    path,
                    def,
                    self.types.get_by_id(type_id).unwrap()
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

    pub fn id_for_type(
        &self,
        def_path: &DefPath,
        type_: &<TypeDefinition as Symbol>::SymbolKey,
    ) -> Result<TypeId, SymbolError> {
        let mut scan_def_path = def_path.clone();
        let type_component: DefPathComponent = Into::<DefPathComponent>::into(type_.clone());
        while !scan_def_path.is_empty() {
            if scan_def_path.can_own(&type_component) {
                let mut component_def_path = scan_def_path.clone();
                component_def_path.push(type_component.clone()).unwrap();
                match self.defs.get(&component_def_path) {
                    Some(SymbolDef::Type(type_id)) => return Ok(*type_id),
                    _ => (),
                }
            }
            scan_def_path.pop();
        }

        Err(SymbolError::Undefined(def_path.clone(), type_component))
    }

    pub fn type_for_id(&self, id: &TypeId) -> Option<&TypeDefinition> {
        self.types.get_by_id(id)
    }

    pub fn insert<S>(&mut self, def_path: DefPath, symbol: S) -> Result<DefPath, SymbolError>
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

        match self.defs.insert(
            full_def_path.clone(),
            Into::<SymbolDef>::into(DefGenerator::new(
                full_def_path.clone(),
                &mut self.types,
                symbol,
            )),
        ) {
            Some(_already_defined) => Err(SymbolError::Defined(def_path)),
            None => Ok(full_def_path),
        }
    }

    fn resolve_use_statements_at_path<S>(
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
        let key_component = Into::<DefPathComponent>::into(key.clone());
        for child_path in self.children.get(def_path).unwrap() {
            if let DefPathComponent::Import(_) = child_path.last() {
                if let SymbolDef::Import(import) = self.defs.get(child_path).unwrap() {
                    if *import.qualified_path.last() == key_component {
                        return Ok(self.lookup_at::<S>(&import.qualified_path).unwrap());
                    }
                }
            }
        }

        Err(SymbolError::Undefined(def_path.clone(), key_component))
    }

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
        let mut scan_def_path = def_path.clone();
        let key_component = Into::<DefPathComponent>::into(key.clone());
        while !scan_def_path.is_empty() {
            if scan_def_path.can_own(&key_component) {
                let component_def_path = scan_def_path
                    .clone()
                    .with_component(key_component.clone())
                    .unwrap();
                match self.defs.get(&component_def_path) {
                    Some(def) => return Ok(<&S>::from(DefResolver::new(&self.types, def))),
                    None => (),
                }
            }
            scan_def_path.pop();
        }

        Err(SymbolError::Undefined(def_path.clone(), key_component))
    }

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
        let mut scan_def_path = def_path.clone();
        let key_component = Into::<DefPathComponent>::into(key.clone());
        while !scan_def_path.is_empty() {
            if scan_def_path.can_own(&key_component) {
                let component_def_path = scan_def_path
                    .clone()
                    .with_component(key_component.clone())
                    .unwrap();
                match self.defs.get(&component_def_path) {
                    Some(def) => {
                        return Ok((
                            <&S>::from(DefResolver::new(&self.types, def)),
                            component_def_path,
                        ))
                    }
                    None => (),
                }
            }
            scan_def_path.pop();
        }

        Err(SymbolError::Undefined(def_path.clone(), key_component))
    }

    fn get_resolver_mut<'a>(&'a mut self, path: &DefPath) -> Option<MutDefResolver<'a>> {
        let def = self.defs.get_mut(path)?;
        Some(MutDefResolver::new(&mut self.types, def))
    }

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

        let defs_ptr = &mut self.defs as *mut BTreeMap<DefPath, SymbolDef>;
        let types_ptr = &mut self.types as *mut TypeInterner;

        while !scan_def_path.is_empty() {
            if scan_def_path.can_own(&key_component) {
                let component_def_path = scan_def_path
                    .clone()
                    .with_component(key_component.clone())
                    .unwrap();

                unsafe {
                    let defs = &mut *defs_ptr;
                    if let Some(symbol) = defs.get_mut(&component_def_path) {
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

    pub fn lookup_at<S>(&self, def_path: &DefPath) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        match self.defs.get(&def_path) {
            Some(def) => return Ok(<&S>::from(DefResolver::new(&self.types, def))),
            None => Err(SymbolError::Undefined(
                def_path.clone(),
                def_path.last().clone(),
            )),
        }
    }

    pub fn lookup_at_mut<S>(&mut self, def_path: &DefPath) -> Result<&mut S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        match self.defs.get_mut(&def_path) {
            Some(def) => return Ok(<&mut S>::from(MutDefResolver::new(&mut self.types, def))),
            None => Err(SymbolError::Undefined(
                def_path.clone(),
                def_path.last().clone(),
            )),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn insert_and_lookup() {
        let mut symtab = SymbolTable::new();

        assert_eq!(
            symtab.insert(DefPath::empty(), Module::new("test_mod".into())),
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
