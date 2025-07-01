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
pub use type_definitions::*;
pub use variable::*;

mod def_path;
pub mod defcontext;
mod errors;
mod function;
pub mod intrinsics;
pub mod symtab_visitor;
mod type_definitions;
mod variable;

pub use def_path::*;
pub use defcontext::*;
pub use symtab_visitor::{MutSymtabVisitor, SymtabVisitor};
pub use TypeRepr;

#[derive(Debug)]
pub enum SymbolDef {
    Module(String),
    Type(TypeId),
    Function(Function),
    Scope(usize),
    Variable(Variable),
    BasicBlock(ir::BasicBlock),
}


impl std::fmt::Display for SymbolDef {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Module(name) => write!(f, "{}", name),
            Self::Type(id) => write!(f, "{}", id),
            Self::Function(function) => write!(f, "{}", function.name()),
            Self::Scope(scope) => write!(f, "scope{}", scope),
            Self::Variable(variable) => write!(f, "{}", variable.name),
            Self::BasicBlock(block) => write!(f, "{}", block.label),
        }
    }
}


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

    fn insert(&mut self, def_path: DefPath, symbol: SymbolDef) -> Result<(), ()>{
        self
            .children
            .entry(def_path.parent())
            .or_default()
            .insert(def_path.clone());

        match self.defs.insert(def_path, symbol) {
            Some(_) => Err(()),
            None => Ok(()),
        }

    }

    fn lookup(&self, mut def_path: DefPath, symbol: &DefPathComponent) -> Option<&SymbolDef> {
        while !def_path.is_empty() {
            let component_def_path = def_path.clone_with_new_last(symbol.clone());
            match self.defs.get(&component_def_path) {
                Some(def) => return Some(def),
                None => (),
            }
            def_path.pop();
        }

        None
    }

    fn lookup_absolute(&self, absolute_def_path: &DefPath) -> Option<&SymbolDef> {
        self.defs.get(absolute_def_path)
    }

    pub fn lookup_module(
        &self,
        def_path: &DefPath,
        name: &str,
    ) -> Result<DefPath, UndefinedSymbol> {
        let full_def_path = def_path.clone_with_new_last(DefPathComponent::Module(name.into()))

        match self.lookup_absolute(&full_def_path) {
            Some(_) => Ok(full_def_path),
            None => Err(UndefinedSymbol::Module(name.into())),
        }
    }

    pub fn next_subscope(&self, def_path: DefPath) -> usize {
        self.children
            .get(&def_path)
            .unwrap()
            .iter()
            .filter(|definition| match definition.last().unwrap() {
                DefPathComponent::Scope(_) => true,
                _ => false,
            })
            .count()
    }

    pub fn create_module(&mut self, def_path: DefPath, name: String) -> Result<(), DefinedSymbol> {
        let module_component = DefPathComponent::Module(name.clone());
        assert!(def_path.can_own(&module_component));
        match self.insert(def_path, SymbolDef::Module(name.clone())) {
            Ok(_) => Ok(()),
            Err(_) => Err(DefinedSymbol::Module(name)),
        }
    }

    pub fn lookup_type(
        &self,
        def_path: &DefPath,
        type_: &types::Type,
    ) -> Result<&TypeDefinition, UndefinedSymbol> {
        match self
            .lookup(def_path.clone(), &DefPathComponent::Type(type_.clone()))
            .ok_or(UndefinedSymbol::Type(type_.clone()))?
        {
            SymbolDef::Type(id) => Ok(self.types.get_by_id(id).unwrap()),
            _ => Err(UndefinedSymbol::Type(type_.clone())),
        }
    }

    pub fn lookup_struct(&self, def_path: DefPath, name: &str) -> Result<&StructRepr, UndefinedSymbol> {
        unimplemented!();
    }

    pub fn create_type(
        &mut self,
        mut def_path: DefPath,
        type_: types::Type,
        definition: TypeDefinition,
        generic_params: GenericParams,
    ) -> Result<(), SymbolError> {
        let type_component = DefPathComponent::Type(type_.clone());
        assert!(def_path.can_own(&type_component));

        def_path.push(type_component);
        let type_key = TypeKey::new(def_path.clone(), generic_params);

        let type_symbol = SymbolDef::Type(self.types.insert(type_key, definition)?);
        self.insert(def_path, type_symbol);
        Ok(())
    }

    pub fn lookup_variable(
        &self,
        def_path: DefPath,
        name: &str,
    ) -> Result<&Variable, UndefinedSymbol> {
        match self
            .lookup(def_path, &DefPathComponent::Variable(name.into()))
            .ok_or(UndefinedSymbol::Variable(name.into()))?
        {
            SymbolDef::Variable(variable) => Ok(variable),
            _ => Err(UndefinedSymbol::Variable(name.into())),
        }
    }

    pub fn create_variable(
        &mut self,
        mut def_path: DefPath,
        variable: Variable,
    ) -> Result<(), DefinedSymbol> {
        let variable_component = DefPathComponent::Variable(variable.name.clone());

        def_path.push(variable_component);
        match self.insert(def_path.clone(), SymbolDef::Variable(variable)) {
            Ok(_) => Ok(()),
            Err(_) => Err(DefinedSymbol::Variable(def_path)),
        }
    }

    pub fn create_function(
        &mut self, 
        mut def_path: DefPath,
        function: Function,
    ) -> Result<(), DefinedSymbol> {
        let function_component = DefPathComponent::Function(function.name().into());

        def_path.push(function_component);
        match self.insert(def_path.clone(), SymbolDef::Function(function.clone())) {
            Ok(_) => Ok(()),
            Err(_) => Err(DefinedSymbol::Function(function.prototype)),
        }
    }
}
