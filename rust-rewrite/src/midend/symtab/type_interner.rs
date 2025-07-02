use crate::midend::symtab::*;

pub type TypeId = usize;
pub type GenericParams = Vec<Type>;

pub struct TypeInterner<'a> {
    ids: HashMap<<TypeDefinition as Symbol<'a>>::SymbolKey, TypeId>,
    types: HashMap<TypeId, TypeDefinition>,
}

impl<'a> TypeInterner<'a> {
    pub fn new() -> Self {
        Self {
            ids: HashMap::new(),
            types: HashMap::new(),
        }
    }

    pub fn insert(
        &'a mut self,
        def_path: DefPath<'a>,
        definition: TypeDefinition,
    ) -> Result<TypeId, SymbolError> {
        assert!(&def_path.is_type());

        let next_id = self.ids.len();
        let key = definition.symbol_key();
        let id = match self.ids.insert(key.clone(), next_id) {
            Some(existing_id) => return Err(SymbolError::Defined(def_path.clone())),
            None => next_id,
        };

        match self.types.insert(next_id, definition) {
            Some(existing_definition) => return Err(SymbolError::Defined(def_path.clone())),
            None => (),
        };

        Ok(next_id)
    }

    pub fn get_by_id(&self, id: &TypeId) -> Option<&TypeDefinition> {
        self.types.get(id)
    }

    pub fn get_by_key(
        &self,
        key: &<TypeDefinition as Symbol<'a>>::SymbolKey,
    ) -> Option<&TypeDefinition> {
        let id_from_key = self.ids.get(key)?;
        self.types.get(id_from_key)
    }
}
