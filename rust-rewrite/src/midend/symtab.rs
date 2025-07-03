use std::collections::{HashMap, HashSet};

use crate::{
    midend::{
        ir,
        types::{self, Type},
    },
    trace,
};
pub use errors::*;
pub use function::*;
pub use serde::Serialize;
pub use variable::*;

mod def_path;
pub mod defcontext;
mod errors;
pub mod intrinsics;
pub mod symbol;
pub mod symtab_visitor;
pub mod type_interner;

pub use def_path::*;
pub use defcontext::*;
pub use module::Module;
pub use scope::Scope;
use symbol::*;
pub use symtab_visitor::{MutSymtabVisitor, SymtabVisitor};
pub use type_interner::*;

pub struct SymbolTable {
    types: TypeInterner,
    defs: HashMap<DefPath, SymbolDef>,
    children: HashMap<DefPath, HashSet<DefPath>>,
}

impl SymbolTable {
    pub fn new() -> Self {
        Self {
            types: TypeInterner::new(),
            defs: HashMap::new(),
            children: HashMap::new(),
        }
    }

    pub fn insert<S>(&mut self, def_path: DefPath, symbol: S) -> Result<(), SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let full_def_path = def_path
            .clone()
            .with_component((symbol.symbol_key()).clone().into());
        self.children
            .entry(def_path.clone())
            .or_default()
            .insert(full_def_path.clone());

        match self.defs.insert(
            full_def_path,
            Into::<SymbolDef>::into(DefGenerator::new(def_path.clone(), &mut self.types, symbol)),
        ) {
            Some(already_defined) => Err(SymbolError::Defined(def_path)),
            None => Ok(()),
        }
    }

    fn lookup<S>(
        &self,
        def_path: DefPath,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let mut scan_def_path = def_path.clone();
        while !scan_def_path.is_empty() {
            let mut component_def_path = scan_def_path.clone();
            component_def_path.push((*key).clone().into());
            match self.defs.get(&component_def_path) {
                Some(def) => return Ok(<&S>::from(DefResolver::new(&self.types, def))),
                None => (),
            }
            scan_def_path.pop();
        }

        Err(SymbolError::Undefined(
            def_path.with_component((*key).clone().into()),
        ))
    }
}

mod tests {
    use super::*;

    #[test]
    fn insert_and_lookup() {
        let mut symtab = SymbolTable::new();

        assert_eq!(
            symtab.insert(DefPath::new(), Module::new("test_mod".into())),
            Ok(())
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
