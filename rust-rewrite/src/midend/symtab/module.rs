/*
*
*
        Module
     | ExternCrate
     | UseDeclaration
     | Function
     | TypeAlias
     | Struct
     | Enumeration
     | Union
     | ConstantItem
     | StaticItem
     | Trait
     | Implementation
     | ExternBlock//
                  */
use crate::midend::{
    symtab::{FunctionOrPrototype, Implementation, TypeDefinition},
    types::Type,
};
use std::collections::HashMap;
pub struct Module {
    pub name: String,
    pub functions: HashMap<String, FunctionOrPrototype>,
    pub type_definitions: HashMap<String, TypeDefinition>,
    pub implementations: HashMap<Type, Implementation>,
    pub modules: HashMap<String, Module>,
}
