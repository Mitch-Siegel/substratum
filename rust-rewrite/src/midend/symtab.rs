use std::collections::HashMap;

use crate::{
    midend::{ir, types::Type},
    trace,
};
pub use errors::*;
pub use function::*;
pub use scope::Scope;
pub use type_definitions::*;
pub use variable::*;

mod errors;
mod function;
mod implementation;
mod module;
mod scope;
mod type_definitions;
mod variable;

pub use implementation::Implementation;
pub use module::Module;
pub use scope::ScopePath;
pub use TypeRepr;

/// Traits for lookup based on ownership of various symbol types
pub trait ScopeOwner {
    fn insert_scope(&mut self, scope: Scope);
}

pub trait BasicBlockOwner {
    fn insert_basic_block(&mut self, block: ir::BasicBlock);
    fn lookup_basic_block(&self, label: usize) -> Option<&ir::BasicBlock>;
    fn lookup_basic_block_mut(&mut self, label: usize) -> Option<&mut ir::BasicBlock>;
}

pub trait VariableOwner {
    fn insert_variable(&mut self, variable: Variable) -> Result<(), DefinedSymbol>;
    fn lookup_variable_by_name(&self, name: &str) -> Result<&Variable, UndefinedSymbol>;
}

pub trait TypeOwner {
    fn insert_type(&mut self, type_: TypeDefinition) -> Result<(), DefinedSymbol>;
    fn lookup_type(&self, type_: &Type) -> Result<&TypeDefinition, UndefinedSymbol>;
    // TODO: remove me? type shouldn't need to be mut if implementations are not stored in the type
    // definition
    fn lookup_type_mut(&mut self, type_: &Type) -> Result<&mut TypeDefinition, UndefinedSymbol>;

    fn lookup_struct(&self, name: &str) -> Result<&StructRepr, UndefinedSymbol>;
}

pub trait FunctionOwner {
    fn insert_function(&mut self, function: Function) -> Result<(), DefinedSymbol>;
    fn lookup_function_prototype(&self, name: &str) -> Result<&FunctionPrototype, UndefinedSymbol>;
    fn lookup_function(&self, name: &str) -> Result<&Function, UndefinedSymbol>;
    fn lookup_function_or_prototype(
        &self,
        name: &str,
    ) -> Result<&FunctionOrPrototype, UndefinedSymbol>;
}

pub trait AssociatedOwner {
    fn insert_associated(&mut self, associated: Function) -> Result<(), DefinedSymbol>;
    // TODO: "maybe you meant..." for associated/method mismatch
    fn lookup_associated(&self, name: &str) -> Result<&Function, UndefinedSymbol>;
}

pub trait MethodOwner {
    fn insert_method(&mut self, method: Function) -> Result<(), DefinedSymbol>;
    // TODO: "maybe you meant..." for associated/method mismatch
    fn lookup_method(&self, name: &str) -> Result<&Function, UndefinedSymbol>;
}

pub trait ModuleOwner {
    fn insert_module(&mut self, module: Module) -> Result<(), DefinedSymbol>;
    fn lookup_module(&self, name: &str) -> Result<&Module, UndefinedSymbol>;
}

pub trait SelfTypeOwner {
    fn self_type(&self) -> &Type;
}

pub trait ScopeOwnerships: BasicBlockOwner + VariableOwner + TypeOwner {}

pub trait ModuleOwnerships: TypeOwner {}

pub trait EnablesTypeSizing: TypeOwner + SelfTypeOwner {}

pub struct SymbolTable {
    pub global_module: Module,
}

impl SymbolTable {
    pub fn new(global_module: Module) -> Self {
        SymbolTable { global_module }
    }

    pub fn collapse_scopes(&mut self) {
        let _ = trace::debug!("SymbolTable::collapse_scopes");
    }
}
