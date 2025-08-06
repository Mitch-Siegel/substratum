use crate::midend::{symtab::*, types::*};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

pub struct Interner {
    ids: HashMap<DefPath, Semantic>,
    types: HashMap<Semantic, TypeDefinition>,
}

impl Interner {
    pub fn new() -> Self {
        Self {
            ids: HashMap::new(),
            types: HashMap::new(),
        }
    }

    fn next_id(&self) -> Semantic {
        Semantic { id: self.ids.len() }
    }

    pub fn insert(
        &mut self,
        def_path: DefPath,
        definition: TypeDefinition,
    ) -> Result<Semantic, SymbolError> {
        assert!(&def_path.is_type());

        let next_id = self.next_id();
        // Ensure that we never overwrite any type
        let overwritten_id = self.ids.insert(def_path.clone(), next_id);
        match overwritten_id {
            Some(_) => return Err(SymbolError::Defined(def_path.clone())),
            None => next_id,
        };

        match self.types.insert(next_id, definition) {
            Some(_) => return Err(SymbolError::Defined(def_path.clone())),
            None => (),
        };

        Ok(next_id)
    }

    pub fn get_semantic(&self, def_path: &DefPath) -> Option<Semantic> {
        self.ids.get(def_path).copied()
    }

    pub fn get_definition(&self, id: &Semantic) -> Option<&TypeDefinition> {
        self.types.get(id)
    }

    pub fn get_definition_mut(&mut self, id: &Semantic) -> Option<&mut TypeDefinition> {
        self.types.get_mut(id)
    }
}
