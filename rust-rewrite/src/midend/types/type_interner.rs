use crate::midend::{types::*, *};
use std::collections::HashMap;

pub mod monomorphization;
pub mod semantic_function;

pub use monomorphization::*;
pub use semantic_function::*;

pub struct Interner {
    type_ids: HashMap<symtab::DefPath, Semantic>,
    function_ids: HashMap<SemanticFunction, Semantic>,
    types: HashMap<Semantic, symtab::TypeDefinition>,
}

impl Interner {
    pub fn new() -> Self {
        Self {
            type_ids: HashMap::new(),
            function_ids: HashMap::new(),
            types: HashMap::new(),
        }
    }

    fn next_id(&self) -> Semantic {
        Semantic {
            id: (self.type_ids.len() + self.function_ids.len()),
        }
    }

    pub fn insert_type(
        &mut self,
        def_path: symtab::DefPath,
        definition: symtab::TypeDefinition,
    ) -> Result<Semantic, symtab::SymbolError> {
        assert!(&def_path.is_type());

        let next_id = self.next_id();
        // Ensure that we never overwrite any type
        let overwritten_id = self.type_ids.insert(def_path.clone(), next_id);
        match overwritten_id {
            Some(_) => return Err(symtab::SymbolError::AlreadyDefined(def_path.clone())),
            None => next_id,
        };

        assert!(self.types.insert(next_id, definition).is_none());
        Ok(next_id)
    }

    pub fn semantic_for_defpath(&self, def_path: &symtab::DefPath) -> Option<Semantic> {
        self.type_ids.get(def_path).copied()
    }

    pub fn get_type_definition(&self, id: &Semantic) -> Option<&symtab::TypeDefinition> {
        self.types.get(id)
    }

    pub fn get_type_definition_mut(
        &mut self,
        id: &Semantic,
    ) -> Option<&mut symtab::TypeDefinition> {
        self.types.get_mut(id)
    }

    pub fn get_syntactic(&self, id: &Semantic) -> Option<&Syntactic> {
        Some(self.types.get(id)?.syntactic())
    }
}
