use std::collections::HashMap;

use serde::Serialize;

use crate::midend::{symtab::*, types::Type};

pub trait CollapseScopes {
    fn collapse_scopes(&mut self, path_from_collapsing_to: ScopePath);
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize)]
pub struct ScopePath {
    indices: Vec<usize>,
}
impl ScopePath {
    pub fn new() -> Self {
        Self {
            indices: Vec::new(),
        }
    }

    pub fn for_new_subscope(mut self, parent: usize) -> Self {
        self.indices.push(parent);
        self
    }

    pub fn empty(&self) -> bool {
        self.indices.len() == 0
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct Scope {
    variables: HashMap<String, Variable>,
    subscopes: Vec<Scope>,
    type_definitions: HashMap<Type, TypeDefinition>,
    basic_blocks: HashMap<usize, ir::BasicBlock>,
}

impl Scope {
    pub fn new() -> Self {
        Scope {
            variables: HashMap::new(),
            subscopes: Vec::new(),
            type_definitions: HashMap::new(),
            basic_blocks: HashMap::new(),
        }
    }
}

impl ScopeOwner for Scope {
    fn insert_scope(&mut self, scope: Scope) {
        self.subscopes.push(scope);
    }
}

impl BasicBlockOwner for Scope {
    fn insert_basic_block(&mut self, block: ir::BasicBlock) {
        match self.basic_blocks.insert(block.label, block) {
            Some(existing_block) => panic!("Basic block {} already exists", existing_block.label),
            None => (),
        }
    }

    fn lookup_basic_block(&self, label: usize) -> Option<&ir::BasicBlock> {
        self.basic_blocks.get(&label)
    }

    fn lookup_basic_block_mut(&mut self, label: usize) -> Option<&mut ir::BasicBlock> {
        self.basic_blocks.get_mut(&label)
    }
}

impl VariableOwner for Scope {
    fn insert_variable(&mut self, variable: Variable) -> Result<(), DefinedSymbol> {
        match self.variables.insert(variable.name.clone(), variable) {
            Some(existing_variable) => Err(DefinedSymbol::variable(existing_variable)),
            None => Ok(()),
        }
    }

    fn lookup_variable_by_name(&self, name: &str) -> Result<&Variable, UndefinedSymbol> {
        self.variables
            .get(name)
            .ok_or(UndefinedSymbol::variable(name.into()))
    }
}

impl TypeOwner for Scope {
    fn insert_type(&mut self, type_: TypeDefinition) -> Result<(), DefinedSymbol> {
        match self.type_definitions.insert(type_.type_().clone(), type_) {
            Some(existing_type) => Err(DefinedSymbol::type_(existing_type.repr)),
            None => Ok(()),
        }
    }

    fn lookup_type(&self, type_: &Type) -> Result<&TypeDefinition, UndefinedSymbol> {
        self.type_definitions
            .get(type_)
            .ok_or(UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_type_mut(&mut self, type_: &Type) -> Result<&mut TypeDefinition, UndefinedSymbol> {
        self.type_definitions
            .get_mut(type_)
            .ok_or(UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_struct(&self, name: &str) -> Result<&StructRepr, UndefinedSymbol> {
        let struct_type = Type::UDT(name.into());

        match self.type_definitions.get(&struct_type) {
            Some(definition) => match &definition.repr {
                TypeRepr::Struct(struct_definition) => return Ok(struct_definition),
            },
            None => {}
        }

        Err(UndefinedSymbol::struct_(name.into()))
    }
}

impl CollapseScopes for Scope {
    fn collapse_scopes(&mut self, path_from_collapsing_to: ScopePath) {
        if !path_from_collapsing_to.empty() {
            let mut index = 0;
            while self.subscopes.len() > 0 {
                let mut subscope = self.subscopes.pop().unwrap();

                let subscope_path = path_from_collapsing_to.clone().for_new_subscope(index);
                subscope.collapse_scopes(subscope_path);

                for (_, mut variable) in subscope.variables {
                    variable.mangle_name_at_index(index);
                    self.variables.insert(variable.name.clone(), variable);
                }

                index += 1;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::midend::symtab::*;
    #[test]
    fn basic_block_owner() {
        let mut s = Scope::new();
        tests::test_basic_block_owner(&mut s);
    }
}
