use crate::midend::ir::value::*;
use std::collections::HashMap;

#[derive(Debug)]
pub enum ValueError {
    NoSuchValueId,
    HasNoType,
    AlreadyHasType(types::Semantic),
}

#[derive(Debug, Clone)]
pub struct ValueInterner {
    values: Vec<Value>,
    ids: HashMap<Value, ValueId>,
    variables: HashMap<symtab::DefPath, ValueId>,
    temp_count: usize,
}

impl ValueInterner {
    pub fn new(unit_type: types::Semantic) -> Self {
        let unit_value = Value::new(ValueKind::Temporary(0), Some(unit_type));

        Self {
            values: vec![unit_value],
            ids: HashMap::new(),
            variables: HashMap::new(),
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

    pub fn next_temp_with_type(&mut self, ty_: types::Semantic) -> ValueId {
        let temp_value = Value::new(ValueKind::Temporary(self.temp_count), Some(ty_));
        self.temp_count += 1;
        self.insert(temp_value).unwrap()
    }

    pub fn value_for_id(&self, id: &ValueId) -> Result<&Value, ValueError> {
        self.values.get(id.index).ok_or(ValueError::NoSuchValueId)
    }

    pub fn value_mut_for_id(&mut self, id: &ValueId) -> Result<&mut Value, ValueError> {
        self.values
            .get_mut(id.index)
            .ok_or(ValueError::NoSuchValueId)
    }

    pub fn semantic_for_id(&self, id: &ValueId) -> Result<types::Semantic, ValueError> {
        self.value_for_id(id)?.ty.ok_or(ValueError::HasNoType)
    }

    pub fn id_for_variable(&mut self, variable_def_path: symtab::DefPath) -> ValueId {
        match self.variables.get(&variable_def_path) {
            Some(id) => *id,
            None => {
                let id = self
                    .insert(Value::new(
                        ValueKind::Variable(variable_def_path.clone()),
                        None,
                    ))
                    .unwrap();
                id
            }
        }
    }

    pub fn def_path_for_id(&self, id: &ValueId) -> Result<Option<&symtab::DefPath>, ValueError> {
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
                        .variables
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
        id: &ValueId,
        ty_: types::Semantic,
    ) -> Result<types::Semantic, ValueError> {
        match self.value_mut_for_id(id)?.ty.replace(ty_) {
            Some(existing_type) => Err(ValueError::AlreadyHasType(existing_type)),
            None => Ok(ty_),
        }
    }
}

impl ValueInterner {
    pub fn diag(&self, symtab: &symtab::SymbolTable) {
        for (v, id) in self.ids.iter() {
            println!("{}: {:?}", id, v);
        }
    }
}
