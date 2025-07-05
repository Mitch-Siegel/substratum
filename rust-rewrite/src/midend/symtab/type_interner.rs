use crate::midend::symtab::*;

pub type TypeId = usize;
pub type GenericParams = Vec<Type>;

#[derive(Debug, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub struct TypeKey {
    def_path: DefPath,
    type_: types::Type,
}

pub struct TypeInterner {
    ids: HashMap<TypeKey, TypeId>,
    types: HashMap<TypeId, TypeDefinition>,
}

impl TypeInterner {
    pub fn new() -> Self {
        Self {
            ids: HashMap::new(),
            types: HashMap::new(),
        }
    }

    pub fn insert(
        &mut self,
        def_path: DefPath,
        definition: TypeDefinition,
    ) -> Result<TypeId, SymbolError> {
        assert!(&def_path.is_type());

        let next_id = self.ids.len();
        let id = match self.ids.insert(
            TypeKey {
                def_path: def_path.clone(),
                type_: definition.type_().clone(),
            },
            next_id,
        ) {
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

    pub fn get_mut_by_id(&mut self, id: &TypeId) -> Option<&mut TypeDefinition> {
        self.types.get_mut(id)
    }

    pub fn get_by_key(
        &self,
        key: &<TypeDefinition as Symbol>::SymbolKey,
    ) -> Option<&TypeDefinition> {
        let id_from_key = self.ids.get(key)?;
        self.types.get(id_from_key)
    }

    pub fn get_mut_by_key(
        &mut self,
        key: &<TypeDefinition as Symbol>::SymbolKey,
    ) -> Option<&mut TypeDefinition> {
        let id_from_key = self.ids.get(key)?;
        self.types.get_mut(id_from_key)
    }
}
