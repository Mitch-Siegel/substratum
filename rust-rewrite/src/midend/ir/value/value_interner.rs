use crate::midend::ir::value::*;
use std::collections::HashMap;

pub struct ValueInterner {
    values: Vec<Value>,
    ids: HashMap<Value, ValueId>,
    variables: HashMap<symtab::DefPath, ValueId>,
    temp_count: usize,
}

impl ValueInterner {
    pub fn new(unit_type_id: symtab::TypeId) -> Self {
        let unit_value = Value::new(ValueKind::Temporary(0), Some(unit_type_id));

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

    pub fn next_temp(&mut self, type_: Option<symtab::TypeId>) -> ValueId {
        let temp_value = Value::new(ValueKind::Temporary(self.temp_count), type_);
        self.temp_count += 1;
        self.insert(temp_value).unwrap()
    }

    pub fn value_for_id(&self, id: &ValueId) -> Option<&Value> {
        self.values.get(id.index)
    }

    pub fn value_mut_for_id(&mut self, id: &ValueId) -> Option<&mut Value> {
        self.values.get_mut(id.index)
    }

    pub fn id_for_variable(&self, variable_def_path: &symtab::DefPath) -> Option<&ValueId> {
        self.variables.get(variable_def_path)
    }

    pub fn id_for_variable_or_insert(&mut self, variable_def_path: symtab::DefPath) -> &ValueId {
        let next_id = self.next_id();
        self.variables.entry(variable_def_path).or_insert(next_id)
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

    pub fn insert(&mut self, value: Value) -> Result<ValueId, ()> {
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
                Ok(new_id)
            }
        }
    }
}
