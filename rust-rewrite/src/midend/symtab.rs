use std::collections::HashMap;

use crate::trace;
pub use errors::*;
pub use function::*;
pub use scope::Scope;
use serde::Serialize;
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
pub use scope::{CollapseScopes, ScopePath, ScopeStack, ScopedLookups, ScopedName};
pub use TypeRepr;

#[derive(Debug, Serialize)]
pub struct SymbolTable {
    pub global_scope: Scope,
    pub functions: HashMap<String, FunctionOrPrototype>,
}

impl SymbolTable {
    pub fn new() -> Self {
        SymbolTable {
            global_scope: Scope::new(),
            functions: HashMap::new(),
        }
    }

    pub fn assign_program_points(&mut self) {
        // TODO: re-enable this when SSA implemented
        // for function in self.functions.values_mut() {
        //     match function {
        //         FunctionOrPrototype::Function(f) => {
        //             f.control_flow_mut().assign_program_points();
        //             let mut reaching_defs = ReachingDefs::new(f.control_flow());
        //             reaching_defs.analyze();
        //             reaching_defs.print();
        //         }
        //         FunctionOrPrototype::Prototype(_) => {}
        //     }
        // }
    }

    pub fn print_ir(&self) {
        for function in self.functions.values() {
            match function {
                FunctionOrPrototype::Function(f) => {
                    println!("{}", f.prototype);
                    f.control_flow.to_graphviz();
                    println!("");
                }
                FunctionOrPrototype::Prototype(_) => {}
            }
        }
    }

    pub fn insert_function(&mut self, function: Function) {
        self.functions.insert(
            function.name().into(),
            FunctionOrPrototype::Function(function),
        );
    }

    pub fn insert_function_prototype(&mut self, prototype: FunctionPrototype) {
        self.functions.insert(
            prototype.name.clone(),
            FunctionOrPrototype::Prototype(prototype),
        );
    }

    pub fn collapse_scopes(&mut self) {
        let _ = trace::debug!("SymbolTable::collapse_scopes");

        for (_, function) in &mut self.functions {
            match function {
                FunctionOrPrototype::Prototype(_) => {}
                FunctionOrPrototype::Function(function) => {
                    function.scope.collapse_scopes(None);
                }
            }
        }
    }
}
