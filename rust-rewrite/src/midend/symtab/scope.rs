use std::collections::{BTreeMap, HashMap};

use serde::Serialize;

use crate::midend::{symtab::*, types::ResolvedType};

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
    type_definitions: HashMap<ResolvedType, ResolvedTypeDefinition>,
    basic_blocks: BTreeMap<usize, ir::BasicBlock>,
}

impl Scope {
    pub fn new() -> Self {
        Scope {
            variables: HashMap::new(),
            subscopes: Vec::new(),
            type_definitions: HashMap::new(),
            basic_blocks: BTreeMap::new(),
        }
    }

    pub fn take_all(
        self,
    ) -> (
        HashMap<String, Variable>,
        Vec<Scope>,
        HashMap<ResolvedType, ResolvedTypeDefinition>,
        BTreeMap<usize, ir::BasicBlock>,
    ) {
        (
            self.variables,
            self.subscopes,
            self.type_definitions,
            self.basic_blocks,
        )
    }

    fn mangle_string_at_index(string: String, index: usize) -> String {
        String::from(format!("{}_{}", index, string))
    }

    // TODO: write this as non-recursive when I'm feeling smart
    // ($5 this stays in here unnoticed until long in the future when I'm running this under a profiler)
    fn collapse_internal(&mut self, path: ScopePath) {
        let _span = trace::span_auto_debug!("Scope::collapse_internal", "{:?}", path);
        trace::debug!("Start collapsing scope");
        let mut index = 0;
        while self.subscopes.len() > 0 {
            let mut subscope = self.subscopes.pop().unwrap();

            let subscope_path = path.clone().for_new_subscope(index);
            subscope.collapse_internal(subscope_path);

            let mut renames: HashMap<String, String> = HashMap::new();
            for (old_name, mut variable) in subscope.variables {
                variable.name = Self::mangle_string_at_index(variable.name, index);
                trace::debug!("Rename variable {} -> {}", old_name, variable.name);
                renames.insert(old_name, variable.name.clone());
                self.variables.insert(variable.name.clone(), variable);
            }

            for (_, mut block) in subscope.basic_blocks {
                for statement in &mut block {
                    for read_operand in statement.read_operand_names_mut() {
                        match renames.get(&read_operand.base_name) {
                            Some(new_name) => {
                                trace::trace!(
                                    "Rename read operand {} -> {}",
                                    read_operand.base_name,
                                    new_name
                                );
                                read_operand.base_name = new_name.clone();
                            }
                            None => {}
                        }
                    }

                    for write_operand in statement.write_operand_names_mut() {
                        match renames.get(&write_operand.base_name) {
                            Some(new_name) => {
                                trace::trace!(
                                    "Rename written operand {} -> {}",
                                    write_operand.base_name,
                                    new_name
                                );
                                write_operand.base_name = new_name.clone();
                            }
                            None => {}
                        }
                    }
                }

                self.insert_basic_block(block);
            }
            index += 1;
        }
        trace::debug!("Done collapsing scope");
    }

    pub fn collapse(&mut self) {
        self.collapse_internal(ScopePath::new());
    }
}

impl ScopeOwner for Scope {
    fn subscopes(&self) -> impl Iterator<Item = &Scope> {
        self.subscopes.iter()
    }
}

impl MutScopeOwner for Scope {
    fn subscopes_mut(&mut self) -> impl Iterator<Item = &mut Scope> {
        self.subscopes.iter_mut()
    }

    fn insert_scope(&mut self, scope: Scope) {
        self.subscopes.push(scope);
    }
}

impl BasicBlockOwner for Scope {
    fn basic_blocks(&self) -> impl Iterator<Item = &ir::BasicBlock> {
        self.basic_blocks.values()
    }

    fn lookup_basic_block(&self, label: usize) -> Option<&ir::BasicBlock> {
        self.basic_blocks.get(&label)
    }
}

impl MutBasicBlockOwner for Scope {
    fn basic_blocks_mut(&mut self) -> impl Iterator<Item = &mut ir::BasicBlock> {
        self.basic_blocks.values_mut()
    }

    fn insert_basic_block(&mut self, block: ir::BasicBlock) {
        match self.basic_blocks.insert(block.label, block) {
            Some(existing_block) => panic!("Basic block {} already exists", existing_block.label),
            None => (),
        }
    }

    fn lookup_basic_block_mut(&mut self, label: usize) -> Option<&mut ir::BasicBlock> {
        self.basic_blocks.get_mut(&label)
    }
}

impl VariableOwner for Scope {
    fn variables(&self) -> impl Iterator<Item = &Variable> {
        self.variables.values()
    }

    fn lookup_variable_by_name(&self, name: &str) -> Result<&Variable, UndefinedSymbol> {
        self.variables
            .get(name)
            .ok_or(UndefinedSymbol::variable(name.into()))
    }
}

impl MutVariableOwner for Scope {
    fn variables_mut(&mut self) -> impl Iterator<Item = &mut Variable> {
        self.variables.values_mut()
    }

    fn insert_variable(&mut self, variable: Variable) -> Result<(), DefinedSymbol> {
        match self.variables.insert(variable.name.clone(), variable) {
            Some(existing_variable) => Err(DefinedSymbol::variable(existing_variable)),
            None => Ok(()),
        }
    }
}

impl TypeOwner for Scope {
    fn types(&self) -> impl Iterator<Item = &ResolvedTypeDefinition> {
        self.type_definitions.values()
    }

    fn lookup_type(
        &self,
        type_: &ResolvedType,
    ) -> Result<&ResolvedTypeDefinition, UndefinedSymbol> {
        self.type_definitions
            .get(type_)
            .ok_or(UndefinedSymbol::type_(type_.clone()))
    }
}

impl MutTypeOwner for Scope {
    fn types_mut(&mut self) -> impl Iterator<Item = &mut ResolvedTypeDefinition> {
        self.type_definitions.values_mut()
    }

    fn insert_type(&mut self, type_: ResolvedTypeDefinition) -> Result<(), DefinedSymbol> {
        match self.type_definitions.insert(type_.type_().clone(), type_) {
            Some(existing_type) => Err(DefinedSymbol::type_(existing_type.repr)),
            None => Ok(()),
        }
    }

    fn lookup_type_mut(
        &mut self,
        type_: &ResolvedType,
    ) -> Result<&mut ResolvedTypeDefinition, UndefinedSymbol> {
        self.type_definitions
            .get_mut(type_)
            .ok_or(UndefinedSymbol::type_(type_.clone()))
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
