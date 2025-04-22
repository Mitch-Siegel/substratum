use crate::midend::types::Type;
use std::collections::HashMap;

use serde::Serialize;
use std::fmt::Display;

use super::ir;

#[derive(Clone, Debug, Serialize)]
pub struct Variable {
    name: String,
    mangled_name: Option<String>,
    type_: Type,
}

impl Display for Variable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {}", self.type_, self.name)
    }
}

impl Variable {
    pub fn new(name: String, type_: Type) -> Self {
        Variable {
            name,
            mangled_name: None,
            type_,
        }
    }

    pub fn add_mangled_name(&mut self, scope_indices: &Vec<usize>) {
        let mut mangled_name = String::new();
        for scope in scope_indices {
            mangled_name.push_str(&(scope.to_string() + &String::from("_")));
        }
        mangled_name.push_str(&self.name.clone());

        match &self.mangled_name {
            Some(current_name) => {
                panic!(
                "Variable {} already has mangled name {}, can't add_mangled_name with new name {}",
                self.name,
                current_name,
                mangled_name)
            }
            None => self.mangled_name.replace(mangled_name),
        };
    }

    pub fn name(&self) -> String {
        match &self.mangled_name {
            Some(mangled_name) => mangled_name.clone(),
            None => self.name.clone(),
        }
    }

    pub fn type_(&self) -> &Type {
        &self.type_
    }
}

#[derive(Debug, Serialize)]
pub struct Struct {
    name: String,
    fields: HashMap<String, Variable>,
    field_order: Vec<String>,
}

impl Struct {
    pub fn new(name: String) -> Self {
        Self {
            name,
            fields: HashMap::new(),
            field_order: Vec::new(),
        }
    }

    pub fn add_field(&mut self, name: String, type_: Type) {
        self.fields
            .insert(name.clone(), Variable::new(name.clone(), type_));
        self.field_order.push(name);
    }

    pub fn get_field(&self, name: &String) -> Option<&Variable> {
        self.fields.get(name)
    }
}

#[derive(Debug, Serialize)]
pub struct Scope {
    subscope_indices: Vec<usize>,
    variables: HashMap<String, Variable>,
    subscopes: Vec<Scope>,
    struct_definitions: HashMap<String, Struct>,
}

impl Scope {
    pub fn new() -> Self {
        Scope {
            subscope_indices: Vec::new(),
            variables: HashMap::new(),
            subscopes: Vec::new(),
            struct_definitions: HashMap::new(),
        }
    }

    pub fn insert_variable(&mut self, mut variable: Variable) {
        variable.add_mangled_name(&self.subscope_indices);
        self.variables.insert(variable.name.clone(), variable);
    }

    pub fn lookup_declared_variable_by_name(&self, name: &str) -> &Variable {
        self.variables
            .get(name)
            .expect(&format!("Use of undeclared variable {}", name))
    }

    pub fn lookup_variable_by_name(&self, name: &str) -> Option<&Variable> {
        self.variables.get(name)
    }

    pub fn insert_subscope(&mut self, subscope: Scope) {
        self.subscopes.push(subscope);
    }

    pub fn insert_struct_definition(&mut self, definition: Struct) {
        self.struct_definitions
            .insert(definition.name.clone(), definition);
    }
}

#[derive(Debug, Serialize)]
pub struct FunctionPrototype {
    pub name: String,
    pub arguments: Vec<Variable>,
    pub return_type: Type,
}

impl Display for FunctionPrototype {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut arguments_string = String::new();
        for argument in &self.arguments {
            if arguments_string.len() > 0 {
                arguments_string = format!("{}, {}", arguments_string, argument);
            } else {
                arguments_string = format!("{}", argument);
            }
        }
        match &self.return_type {
            Type::Unit => write!(f, "fun {}({})", self.name, arguments_string),
            _ => write!(
                f,
                "fun {}({}) -> {}",
                self.name, arguments_string, self.return_type
            ),
        }
    }
}

impl FunctionPrototype {
    pub fn new(name: String, arguments: Vec<Variable>, return_type: Type) -> Self {
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
            argument_scope.insert_variable(arg.clone())
        }

        argument_scope
    }
}

#[derive(Debug, Serialize)]
pub struct Function {
    pub prototype: FunctionPrototype,
    pub scope: Scope,
    pub control_flow: ir::ControlFlow,
}

impl Function {
    pub fn new(prototype: FunctionPrototype, scope: Scope, control_flow: ir::ControlFlow) -> Self {
        Function {
            prototype,
            scope,
            control_flow,
        }
    }

    pub fn name(&self) -> String {
        self.prototype.name.clone()
    }
}

#[derive(Debug, Serialize)]
pub enum FunctionOrPrototype {
    Function(Function),
    Prototype(FunctionPrototype),
}

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
