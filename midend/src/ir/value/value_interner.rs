use std::{collections::HashMap, hash};

use crate::ir::value::{Value, ValueId, ValueKind, symtab, types};

#[derive(Debug)]
pub(crate) enum ValueError {
    NoSuchValueId(ValueId),
    #[allow(unused)]
    IdHasNoType(ValueId),
    #[allow(unused)]
    IdAlreadyHasType(ValueId),
}

impl std::fmt::Display for ValueError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::NoSuchValueId(id) => write!(f, "no such value id ({id})"),
            Self::IdHasNoType(id) => write!(f, "value id ({id}) has no type"),
            Self::IdAlreadyHasType(id) => write!(f, "value id {id} already has type"),
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct ValueInterner<T>
where
    T: Clone + Eq + hash::Hash,
{
    values: Vec<Value<T>>,
    ids: HashMap<Value<T>, ValueId>,
    pathed_ids: HashMap<symtab::ValuePath, ValueId>,
    temp_count: usize,
}

impl ValueInterner<Option<types::Syntactic>> {
    pub(crate) fn next_temp(&mut self) -> ValueId {
        let temp_value = Value::new(ValueKind::Temporary(self.temp_count), None);
        self.temp_count += 1;
        self.insert(temp_value).unwrap()
    }

    /// given the `DefPath`, return its `ValueID`. Requires &mut self as this method may
    /// generate a new `ValueId` if one does not already exist for the variable
    pub(crate) fn id_for_path(&mut self, def_path: &symtab::ValuePath) -> ValueId {
        match self.pathed_ids.get(def_path) {
            Some(id) => *id,
            None => self
                .insert(Value::new(ValueKind::Variable(def_path.clone()), None))
                .unwrap(),
        }
    }

    #[allow(unused)]
    pub(crate) fn assign_type_to_id(
        &mut self,
        val: ValueId,
        ty: types::Syntactic,
    ) -> Result<(), ValueError> {
        match self.value_mut_for_id(val)?.ty.replace(ty) {
            Some(_existing_type) => Err(ValueError::IdAlreadyHasType(val)),
            None => Ok(()),
        }
    }

    pub(crate) fn id_for_constant(&mut self, constant: usize) -> &ValueId {
        let constant_value = Value::new(ValueKind::Constant(constant), Some(types::Syntactic::U64));
        let next_id = self.next_id();
        self.ids.entry(constant_value).or_insert(next_id)
    }
}

impl<T> ValueInterner<T>
where
    T: Clone + Eq + hash::Hash,
{
    pub(crate) fn new(unit_type: T) -> Self {
        let unit_value = Value::new(ValueKind::Temporary(0), unit_type);

        Self {
            values: vec![unit_value],
            ids: HashMap::new(),
            pathed_ids: HashMap::new(),
            temp_count: 1,
        }
    }

    pub(crate) fn unit_value_id() -> ValueId {
        ValueId::new(0)
    }

    /// given a `ValueId`, return a reference to the full backing Value (or `NoSuchValueId` error
    /// if not interned)
    pub(crate) fn value_for_id(&self, val: ValueId) -> Result<&Value<T>, ValueError> {
        self.values
            .get(val.index)
            .ok_or(ValueError::NoSuchValueId(val))
    }

    /// given a `ValueId`, return a mutable reference to the full backing value (or `NoSuchValueId`
    /// error if not interned)
    #[allow(unused)]
    pub(crate) fn value_mut_for_id(&mut self, val: ValueId) -> Result<&mut Value<T>, ValueError> {
        self.values
            .get_mut(val.index)
            .ok_or(ValueError::NoSuchValueId(val))
    }

    /// given a `ValueId`, return the semantic type of the value (or `HasNoType` error if type is
    /// unknown)
    #[allow(unused)]
    pub(crate) fn type_for_id(&self, val: ValueId) -> Result<&T, ValueError> {
        self.value_for_id(val).map(|v| &v.ty)
    }

    /// given a `ValueId`, return an option containing the `DefPath` of the associated variable, or
    /// None if the backing value has a kind other than Variable. Returns `NoSuchValueId` in
    /// error cases
    #[allow(unused)]
    pub(crate) fn def_path_for_id(
        &self,
        id: ValueId,
    ) -> Result<Option<&symtab::ValuePath>, ValueError> {
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

    fn insert(&mut self, value: Value<T>) -> Result<ValueId, ()> {
        if self.ids.contains_key(&value) {
            Err(())
        } else {
            let new_id = self.next_id();
            if let ValueKind::Variable(variable_path) = &value.kind {
                assert!(
                    self.pathed_ids
                        .insert(variable_path.clone(), new_id)
                        .is_none()
                );
            }
            self.ids.insert(value.clone(), new_id);
            self.values.push(value);
            Ok(new_id)
        }
    }

    pub(crate) fn ids(&self) -> impl Iterator<Item = (&Value<T>, &ValueId)> {
        self.ids.iter()
    }
}
