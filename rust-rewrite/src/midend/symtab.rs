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

pub trait Symbol<'a>
where
    &'a Self: From<DefinitionResolver<'a>>,
    Self: 'a,
    Self::SymbolKey: Clone,
    SymbolDefGenerator<'a, Self>: Into<SymbolDef>,
{
    type SymbolKey: Into<DefPathComponent<'a>>;

    fn symbol_key(&self) -> &Self::SymbolKey;
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ModuleName(String);
impl std::fmt::Display for ModuleName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Module {
    name: ModuleName,
}
impl<'a> From<DefinitionResolver<'a>> for &'a Module {
    fn from(resolver: DefinitionResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Module(module) => module,
            symbol => panic!("Unexpected symbol seen for module: {}", symbol),
        }
    }
}

impl<'a> Into<DefPathComponent<'a>> for &Module {
    fn into(self) -> DefPathComponent<'a> {
        DefPathComponent::Module(self.symbol_key().clone())
    }
}
impl<'a> Into<SymbolDef> for SymbolDefGenerator<'a, Module> {
    fn into(self) -> SymbolDef {
        SymbolDef::Module(self.to_generate_def_for)
    }
}
impl<'a> Symbol<'a> for Module {
    type SymbolKey = ModuleName;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.name
    }
}
impl std::fmt::Display for Module {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ScopeIndex(usize);
impl std::fmt::Display for ScopeIndex {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Scope {
    index: ScopeIndex,
}
impl<'a> From<DefinitionResolver<'a>> for &'a Scope {
    fn from(resolver: DefinitionResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Scope(scope) => scope,
            symbol => panic!("Unexpected symbol seen for scope: {}", symbol),
        }
    }
}

impl<'a> Into<DefPathComponent<'a>> for &Scope {
    fn into(self) -> DefPathComponent<'a> {
        DefPathComponent::Scope(self.symbol_key().clone())
    }
}

impl<'a> Into<SymbolDef> for SymbolDefGenerator<'a, Scope> {
    fn into(self) -> SymbolDef {
        SymbolDef::Scope(self.to_generate_def_for)
    }
}

impl<'a> Into<SymbolDef> for Scope {
    fn into(self) -> SymbolDef {
        SymbolDef::Scope(self)
    }
}
impl<'a> Symbol<'a> for Scope {
    type SymbolKey = ScopeIndex;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.index
    }
}
impl std::fmt::Display for Scope {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.index)
    }
}

pub struct DefinitionResolver<'a> {
    pub type_interner: &'a TypeInterner<'a>,
    pub to_resolve: &'a SymbolDef,
}
impl<'a> DefinitionResolver<'a> {
    pub fn new(type_interner: &'a TypeInterner<'a>, to_resolve: &'a SymbolDef) -> Self {
        Self {
            type_interner,
            to_resolve,
        }
    }
}

pub struct SymbolDefGenerator<'a, S>
where
    S: Symbol<'a>,
    &'a S: From<DefinitionResolver<'a>>,
{
    pub def_path: DefPath<'a>,
    pub type_interner: &'a mut TypeInterner<'a>,
    pub to_generate_def_for: S,
}

impl<'a, S> SymbolDefGenerator<'a, S>
where
    S: Symbol<'a>,
    &'a S: From<DefinitionResolver<'a>>,
{
    pub fn new(
        def_path: DefPath<'a>,
        type_interner: &'a mut TypeInterner<'a>,
        to_generate_def_for: S,
    ) -> Self {
        Self {
            def_path,
            type_interner,
            to_generate_def_for,
        }
    }
}

#[derive(Debug)]
pub enum SymbolDef {
    Module(Module),
    Type(TypeId),
    Function(Function),
    Scope(Scope),
    Variable(Variable),
    BasicBlock(ir::BasicBlock),
}

impl std::fmt::Display for SymbolDef {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Module(module) => write!(f, "{}", module),
            Self::Type(id) => write!(f, "{}", id),
            Self::Function(function) => write!(f, "{}", function.name()),
            Self::Scope(scope) => write!(f, "scope{}", scope),
            Self::Variable(variable) => write!(f, "{}", variable.name),
            Self::BasicBlock(block) => write!(f, "{}", block.label),
        }
    }
}

pub struct SymbolTable<'a> {
    types: TypeInterner<'a>,
    defs: HashMap<DefPath<'a>, SymbolDef>,
    children: HashMap<DefPath<'a>, HashSet<DefPath<'a>>>,
}

impl<'a> SymbolTable<'a> {
    pub fn new() -> Self {
        Self {
            types: TypeInterner::new(),
            defs: HashMap::new(),
            children: HashMap::new(),
        }
    }

    pub fn insert<S>(&mut self, def_path: DefPath, symbol: S) -> Result<(), SymbolError>
    where
        S: Symbol<'a>,
        &'a S: From<DefinitionResolver<'a>>,
        SymbolDefGenerator<'a, S>: Into<SymbolDef>,
    {
        let full_def_path = def_path.with_component((symbol.symbol_key()).clone().into());
        self.children
            .entry(def_path)
            .or_default()
            .insert(full_def_path.clone());

        match self.defs.insert(
            full_def_path,
            Into::<SymbolDef>::into(SymbolDefGenerator::new(def_path, &mut self.types, symbol)),
        ) {
            Some(already_defined) => Err(SymbolError::Defined(def_path)),
            None => Ok(()),
        }
    }

    fn lookup<S>(
        &'a self,
        def_path: DefPath,
        key: &<S as Symbol<'a>>::SymbolKey,
    ) -> Result<&'a S, SymbolError>
    where
        S: Symbol<'a>,
        &'a S: From<DefinitionResolver<'a>>,
        SymbolDefGenerator<'a, S>: Into<SymbolDef>,
    {
        let mut scan_def_path = def_path.clone();
        while !scan_def_path.is_empty() {
            let mut component_def_path = scan_def_path.clone();
            component_def_path.push((*key).clone().into());
            match self.defs.get(&component_def_path) {
                Some(def) => {
                    return Ok(<&S>::from(DefinitionResolver::<'a>::new(&self.types, def)))
                }
                None => (),
            }
            scan_def_path.pop();
        }

        Err(SymbolError::Undefined(
            def_path.with_component((*key).clone().into()),
        ))
    }
}
