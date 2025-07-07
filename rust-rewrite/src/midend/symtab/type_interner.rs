use crate::midend::symtab::*;

#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub struct TypeId {
    id: usize,
}

impl std::fmt::Display for TypeId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.id)
    }
}

pub type GenericParams = Vec<Type>;

#[derive(PartialOrd, Ord, PartialEq, Eq, Hash)]
pub struct TypeKey {
    def_path: DefPath,
}

impl TypeKey {
    pub fn new(def_path: DefPath) -> Self {
        assert!(def_path.is_type());
        Self { def_path }
    }
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
        let id = match self.ids.insert(TypeKey::new(def_path.clone()), next_id) {
            Some(_) => return Err(SymbolError::Defined(def_path.clone())),
            None => next_id,
        };

        match self.types.insert(next_id, definition) {
            Some(_) => return Err(SymbolError::Defined(def_path.clone())),
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

    pub fn get(&self, key: &TypeKey) -> Option<&TypeDefinition> {
        let id_from_key = self.ids.get(key)?;
        self.types.get(id_from_key)
    }

    pub fn get_mut(&mut self, key: &TypeKey) -> Option<&mut TypeDefinition> {
        let id_from_key = self.ids.get(key)?;
        self.types.get_mut(id_from_key)
    }
}
