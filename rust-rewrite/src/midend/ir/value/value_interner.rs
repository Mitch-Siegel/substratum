use crate::midend::{ir::value::*, ir::*, *};
use std::collections::BTreeMap;

pub struct ValueInterner {
    values: Vec<Value>,
    ids_by_kind: BTreeMap<ValueKind, ValueId>,
    ids_by_value: BTreeMap<Value, ValueId>,
}

impl ValueInterner {
    pub fn new(unit_type_id: symtab::TypeId) -> Self {
        let unit_value = Value::new(ValueKind::Temporary, Some(unit_type_id));

        Self {
            values: vec![unit_value],
            ids_by_kind: vec![],
            ids: vec![0, unit_value].into_iter().collect::<BTreeMap>(),
        }
    }

    pub fn unit_value_id() -> ValueId {
        ValueId::new(0)
    }

    pub fn get(&self, id: &ValueId) -> Option<&Value> {
        self.values.get(id.index)
    }

    pub fn get_mut(&self, id: &ValueId) -> Option<&Value> {
        self.values.get_mut(id.index)
    }

    pub fn insert(&mut self, value: Value) -> Result<ValueId, ()> {
        match self.ids.get(value) {
            Some(existing_id) => Err(()),
            None => {
                let new_id = ValueId::new(self.values.len());
                self.ids.insert(value.clone(), new_id);
                self.values.push(value);
                Ok(new_id)
            }
        }
    }
}
