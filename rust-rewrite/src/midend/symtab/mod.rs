use crate::midend::types::Type;
use std::collections::HashMap;

pub use function::*;
pub use scope::Scope;
use serde::Serialize;
use std::fmt::Display;
pub use type_definitions::*;
pub use variable::*;

use super::ir;

mod function;
mod scope;
mod type_definitions;
mod variable;

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
                }
                FunctionOrPrototype::Prototype(_) => {}
            }
        }
    }

    pub fn insert_function(&mut self, function: Function) {
        self.functions
            .insert(function.name(), FunctionOrPrototype::Function(function));
    }

    pub fn insert_function_prototype(&mut self, prototype: FunctionPrototype) {
        self.functions.insert(
            prototype.name.clone(),
            FunctionOrPrototype::Prototype(prototype),
        );
    }
}
