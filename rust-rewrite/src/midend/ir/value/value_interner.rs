use crate::midend::ir::value::*;
use std::collections::HashMap;

#[derive(Debug)]
pub enum ValueError {
    NoSuchValueId(ValueId),
    IdHasNoType(ValueId),
    ValueHasNoType,
    ValueAlreadyHasType(types::Semantic),
    IdAlreadyHasType(ValueId, types::Semantic),
}

impl std::fmt::Display for ValueError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::NoSuchValueId(id) => write!(f, "no such value id ({})", id),
            Self::IdHasNoType(id) => write!(f, "value id ({}) has no type", id),
            Self::ValueHasNoType => write!(f, "value has no type"),
            Self::ValueAlreadyHasType(ty) => write!(f, "value already has type {}", ty),
            Self::IdAlreadyHasType(id, ty) => write!(f, "value id {} already has type {}", id, ty),
        }
    }
}

#[derive(Debug, Clone)]
pub struct ValueInterner {
    values: Vec<Value>,
    ids: HashMap<Value, ValueId>,
    pathed_ids: HashMap<symtab::RawPath, ValueId>,
    temp_count: usize,
}

impl ValueInterner {
    pub fn new(unit_type: types::Semantic) -> Self {
        let unit_value = Value::new(ValueKind::Temporary(0), Some(unit_type));

        Self {
            values: vec![unit_value],
            ids: HashMap::new(),
            pathed_ids: HashMap::new(),
            temp_count: 1,
        }
    }

    pub fn unit_value_id() -> ValueId {
        ValueId::new(0)
    }

    pub fn next_temp(&mut self) -> ValueId {
        let temp_value = Value::new(ValueKind::Temporary(self.temp_count), None);
        self.temp_count += 1;
        self.insert(temp_value).unwrap()
    }

    pub fn next_temp_with_type(&mut self, ty: types::Semantic) -> ValueId {
        let temp_value = Value::new(ValueKind::Temporary(self.temp_count), Some(ty));
        self.temp_count += 1;
        self.insert(temp_value).unwrap()
    }

    /// given a ValueId, return a reference to the full backing Value (or NoSuchValueId error
    /// if not interned)
    pub fn value_for_id(&self, val: &ValueId) -> Result<&Value, ValueError> {
        self.values
            .get(val.index)
            .ok_or(ValueError::NoSuchValueId(*val))
    }

    /// given a ValueId, return a mutable reference to the full backing value (or NoSuchValueId
    /// error if not interned)
    pub fn value_mut_for_id(&mut self, val: &ValueId) -> Result<&mut Value, ValueError> {
        self.values
            .get_mut(val.index)
            .ok_or(ValueError::NoSuchValueId(*val))
    }

    /// given a ValueId, return the semantic type of the value (or HasNoType error if type is
    /// unknown)
    pub fn semantic_for_id(&self, val: &ValueId) -> Result<types::Semantic, ValueError> {
        self.value_for_id(val)?
            .ty
            .ok_or(ValueError::IdHasNoType(*val))
    }

    /// given the DefPath, return its ValueID. Requires &mut self as this method may
    /// generate a new ValueId if one does not already exist for the variable
    pub fn id_for_path(&mut self, def_path: symtab::RawPath) -> ValueId {
        match self.pathed_ids.get(&def_path) {
            Some(id) => *id,
            None => self
                .insert(Value::new(ValueKind::Variable(def_path.clone()), None))
                .unwrap(),
        }
    }

    /// given a ValueId, return an option containing the DefPath of the associated variable, or
    /// None if the backing value has a kind other than Variable. Returns NoSuchValueId in
    /// error cases
    pub fn def_path_for_id(&self, id: &ValueId) -> Result<Option<&symtab::RawPath>, ValueError> {
        match &self.value_for_id(id)?.kind {
            ValueKind::Variable(def_path) => Ok(Some(def_path)),
            _ => Ok(None),
        }
    }

    fn next_id(&self) -> ValueId {
        ValueId {
            index: self.ids.len(),
        }
    }

    pub fn id_for_constant(&mut self, constant: usize) -> &ValueId {
        let constant_value = Value::new(ValueKind::Constant(constant), None);
        let next_id = self.next_id();
        self.ids.entry(constant_value).or_insert(next_id)
    }

    fn insert(&mut self, value: Value) -> Result<ValueId, ()> {
        match self.ids.get(&value) {
            Some(_) => Err(()),
            None => {
                let new_id = self.next_id();
                if let ValueKind::Variable(variable_path) = &value.kind {
                    assert!(self
                        .pathed_ids
                        .insert(variable_path.clone(), new_id)
                        .is_none());
                }
                self.ids.insert(value.clone(), new_id);
                self.values.push(value);
                Ok(new_id)
            }
        }
    }
}

/// Type addition to existing values
impl ValueInterner {
    pub fn assign_type_to_id(
        &mut self,
        val: &ValueId,
        ty: types::Semantic,
    ) -> Result<types::Semantic, ValueError> {
        match self.value_mut_for_id(val)?.ty.replace(ty) {
            Some(existing_type) => Err(ValueError::IdAlreadyHasType(*val, existing_type)),
            None => Ok(ty),
        }
    }
}

impl ValueInterner {
    pub fn diag(&self, _symtab: &symtab::SymbolTable) {
        for (v, id) in self.ids.iter() {
            println!("{}: {:?}", id, v);
        }
    }
}
