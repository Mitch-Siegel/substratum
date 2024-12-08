use super::ir::{self, BasicBlock, ControlFlow};
use crate::midend::types::Type;
use std::{collections::HashMap, hash::Hash};

use serde::Serialize;

#[derive(Clone, Debug, Serialize)]
pub struct Variable {
    name: String,
    type_: Type,
}

impl Variable {
    pub fn new(name: String, type_: Type) -> Self {
        Variable { name, type_ }
    }

    pub fn type_(&self) -> Type {
        self.type_
    }
}

#[derive(Debug, Serialize)]
pub struct Scope {
    variables: HashMap<String, Variable>,
    subscopes: Vec<Scope>,
    basic_blocks: HashMap<usize, BasicBlock>,
}

impl Scope {
    pub fn new() -> Self {
        Scope {
            variables: HashMap::new(),
            subscopes: Vec::new(),
            basic_blocks: HashMap::new(),
        }
    }

    pub fn insert_variable(&mut self, variable: Variable) {
        self.variables.insert(variable.name.clone(), variable);
    }

    pub fn lookup_variable_by_name(&self, name: &str) -> Option<&Variable> {
        self.variables.get(name)
    }

    pub fn insert_subscope(&mut self, subscope: Scope) {
        self.subscopes.push(subscope);
    }

    pub fn insert_basic_block(&mut self, block: BasicBlock) {
        self.basic_blocks.insert(block.label(), block);
    }

    pub fn lookup_basic_block(&self, label: usize) -> Option<&BasicBlock> {
        self.basic_blocks.get(&label)
    }

    pub fn lookup_basic_block_mut(&mut self, label: usize) -> Option<&mut BasicBlock> {
        self.basic_blocks.get_mut(&label)
    }
}

#[derive(Debug, Serialize)]
pub struct FunctionPrototype {
    name: String,
    arguments: Vec<Variable>,
    return_type: Option<Type>,
}

impl FunctionPrototype {
    pub fn new(name: String, arguments: Vec<Variable>, return_type: Option<Type>) -> Self {
        FunctionPrototype {
            name,
            arguments,
            return_type,
        }
    }

    pub fn create_argument_scope(&mut self) -> Scope {
        let mut arg_names: Vec<String> = Vec::new();
        let mut argument_scope = Scope::new();
        for arg in &self.arguments {
            arg_names.push(arg.name.clone());
            argument_scope.insert_variable(arg.clone());
        }

        argument_scope
    }
}

#[derive(Debug, Serialize)]
pub struct Function {
    prototype: FunctionPrototype,
    scope: Scope,
    control_flow: ControlFlow,
}

impl Function {
    pub fn new(prototype: FunctionPrototype, scope: Scope, control_flow: ControlFlow) -> Self {
        Function {
            prototype,
            scope,
            control_flow,
        }
    }

    pub fn name(&self) -> String {
        self.prototype.name.clone()
    }

    pub fn prototype(&self) -> &FunctionPrototype {
        &self.prototype
    }

    pub fn control_flow(&self) -> &ControlFlow {
        &self.control_flow
    }

    pub fn get_basic_block(&self, label: usize) -> &BasicBlock {
        self.scope.lookup_basic_block(label).expect(
            format!(
                "Function::lookup_basic_block for nonexistent block {}",
                label
            )
            .as_str(),
        )
    }
}

#[derive(Debug, Serialize)]
enum FunctionOrPrototype {
    Function(Function),
    Prototype(FunctionPrototype),
}

#[derive(Debug, Serialize)]
pub struct SymbolTable {
    global_scope: Scope,
    functions: HashMap<String, FunctionOrPrototype>,
}

impl SymbolTable {
    pub fn new() -> Self {
        SymbolTable {
            global_scope: Scope::new(),
            functions: HashMap::new(),
        }
    }

    pub fn print_ir(&self) {
        for function in self.functions.values() {
            match function {
                FunctionOrPrototype::Function(f) => {
                    println!("{}", serde_json::to_string_pretty(f.prototype()).unwrap());
                    let cf = f.control_flow();
                    for block_idx in 0..cf.block_count() {
                        let bb = f.get_basic_block(block_idx);
                        println!("Block {}:", block_idx);
                        println!("{}", serde_json::to_string_pretty(&bb).unwrap());
                    }
                }
                FunctionOrPrototype::Prototype(p) => {}
            }
        }
    }
}

pub trait InsertFunction {
    fn insert_function(&mut self, function: Function);
}

impl InsertFunction for SymbolTable {
    fn insert_function(&mut self, function: Function) {
        self.functions
            .insert(function.name(), FunctionOrPrototype::Function(function));
    }
}

impl SymbolTable {
    pub fn insert_function_prototype(&mut self, prototype: FunctionPrototype) {
        self.functions.insert(
            prototype.name.clone(),
            FunctionOrPrototype::Prototype(prototype),
        );
    }
}
