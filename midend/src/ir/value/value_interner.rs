use std::{
    collections::{HashMap, HashSet},
    hash,
};

use crate::ir::value::{Value, ValueId, ValueKind, ValueMut, symtab, types};

pub(super) mod ssa_value;
pub(super) use ssa_value::{SsaValue, SsaValueKind};

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
    T: Eq + Ord + hash::Hash,
{
    values: Vec<SsaValue<T>>,
    ssa_instances: HashMap<ValueId, HashSet<ValueId>>,
    pathed_ids: HashMap<symtab::ValuePath, ValueId>,
    constant_ids: HashMap<usize, ValueId>,
    temp_count: usize,
}

impl ValueInterner<Option<types::Syntactic>> {
    pub(crate) fn next_temp(
        &mut self,
        #[cfg(feature = "value_locs")] loc: frontend::sourceloc::StaticSourceLoc,
    ) -> ValueId {
        let temp_value = SsaValue::new(
            ValueKind::Temporary(self.temp_count).into(),
            None,
            #[cfg(feature = "value_locs")]
            loc,
        );

        self.temp_count += 1;
        self.insert(temp_value)
    }

    /// given the `DefPath`, return its `ValueID`. Requires &mut self as this method may
    /// generate a new `ValueId` if one does not already exist for the variable
    pub(crate) fn id_for_path(
        &mut self,
        def_path: &symtab::ValuePath,
        #[cfg(feature = "value_locs")] loc: frontend::sourceloc::StaticSourceLoc,
    ) -> ValueId {
        match self.pathed_ids.get(def_path) {
            Some(id) => *id,
            None => self.insert(SsaValue::new(
                ValueKind::Variable(def_path.clone()).into(),
                None,
                #[cfg(feature = "value_locs")]
                loc,
            )),
        }
    }

    #[allow(unused)]
    pub(crate) fn assign_type_to_id(
        &mut self,
        val: ValueId,
        ty: types::Syntactic,
    ) -> Result<(), ValueError> {
        dbg!(
            "assign type {ty} to id {val}, which currently has {:?}",
            self.value_mut_for_id(val)
        );
        match self.value_mut_for_id(val)?.ty.replace(ty) {
            Some(_existing_type) => Err(ValueError::IdAlreadyHasType(val)),
            None => Ok(()),
        }
    }

    pub(crate) fn id_for_constant(
        &mut self,
        constant: usize,
        #[cfg(feature = "value_locs")] loc: frontend::sourceloc::StaticSourceLoc,
    ) -> ValueId {
        if !self.constant_ids.contains_key(&constant) {
            let constant_value = SsaValue::new(
                ValueKind::Constant(constant).into(),
                Some(types::Syntactic::U64),
                #[cfg(feature = "value_locs")]
                loc,
            );
            self.insert(constant_value);
        }

        *self.constant_ids.get(&constant).unwrap()
    }
}

impl<T> ValueInterner<T>
where
    T: Clone + Eq + Ord + hash::Hash,
{
    pub(crate) fn new(
        unit_type: T,
        #[cfg(feature = "value_locs")] loc: frontend::sourceloc::StaticSourceLoc,
    ) -> Self {
        let unit_value = SsaValue::new(
            ValueKind::Temporary(0).into(),
            unit_type,
            #[cfg(feature = "value_locs")]
            loc,
        );

        Self {
            values: vec![unit_value],
            ssa_instances: HashMap::new(),
            pathed_ids: HashMap::new(),
            constant_ids: HashMap::new(),
            temp_count: 1,
        }
    }

    pub(crate) fn unit_value_id() -> ValueId {
        ValueId::new(0)
    }

    /// given a `ValueId`, return a reference to the full backing Value (or `NoSuchValueId` error
    /// if not interned)
    pub(crate) fn value_for_id(&self, val: ValueId) -> Result<Value<'_, T>, ValueError> {
        self.values
            .get(val.index)
            .ok_or(ValueError::NoSuchValueId(val))
            .map(|ssa| ssa.try_into().unwrap())
    }

    pub(crate) fn ssa_value_for_id(&self, val: ValueId) -> Result<&SsaValue<T>, ValueError> {
        self.values
            .get(val.index)
            .ok_or(ValueError::NoSuchValueId(val))
    }

    /// given a `ValueId`, return a mutable reference to the full backing value (or `NoSuchValueId`
    /// error if not interned)
    #[allow(unused)]
    pub(crate) fn value_mut_for_id(&mut self, val: ValueId) -> Result<ValueMut<'_, T>, ValueError> {
        self.values
            .get_mut(val.index)
            .ok_or(ValueError::NoSuchValueId(val))
            .map(|ssa| ssa.try_into().unwrap())
    }

    /// given a `ValueId`, return the semantic type of the value (or `HasNoType` error if type is
    /// unknown)
    #[allow(unused)]
    pub(crate) fn type_for_id(&self, val: ValueId) -> Result<&T, ValueError> {
        self.value_for_id(val).map(|v| v.ty)
    }

    /// given a `ValueId`, return an option containing the `DefPath` of the associated variable, or
    /// None if the backing value has a kind other than Variable. Returns `NoSuchValueId` in
    /// error cases
    #[allow(unused)]
    pub(crate) fn def_path_for_id(
        &self,
        id: ValueId,
    ) -> Result<Option<&symtab::ValuePath>, ValueError> {
        let base = self.ssa_base_value(id)?;
        match &self.value_for_id(id)?.kind {
            ValueKind::Variable(def_path) => Ok(Some(def_path)),
            _ => Ok(None),
        }
    }

    fn insert(&mut self, value: SsaValue<T>) -> ValueId {
        let new_id = ValueId::new(self.values.len());
        match &value.kind {
            SsaValueKind::Base(ValueKind::Variable(path)) => {
                assert!(self.pathed_ids.insert(path.clone(), new_id).is_none());
            }
            SsaValueKind::Base(ValueKind::Constant(constant)) => {
                assert!(self.constant_ids.insert(*constant, new_id).is_none());
            }
            SsaValueKind::Instance { base_id, instance } => {
                assert!(self.values.get(base_id.index).is_some());
                let ssa_instances = self.ssa_instances.entry(*base_id).or_default();
                assert_eq!(ssa_instances.len(), *instance);
                ssa_instances.insert(new_id);
            }
            SsaValueKind::Base(_) => (),
        }

        self.values.push(value);
        new_id
    }

    // TODO: return ids only, not values
    pub(crate) fn ids(&self) -> impl Iterator<Item = ValueId> {
        self.values
            .iter()
            .enumerate()
            .map(|(idx, _val)| ValueId::new(idx))
    }

    pub(crate) fn kind_of(&self, id: ValueId) -> Result<&ValueKind, ValueError> {
        let base = self.ssa_base_value(id)?;
        Ok(base.kind)
    }
}

impl<T> ValueInterner<T>
where
    T: Clone + Eq + Ord + hash::Hash,
{
    pub(crate) fn ssa_base_id(&self, id: ValueId) -> Result<ValueId, ValueError> {
        let value = self
            .values
            .get(id.index)
            .ok_or(ValueError::NoSuchValueId(id))?;
        match value.kind {
            SsaValueKind::Base(_) => Ok(id),
            SsaValueKind::Instance { base_id, .. } => {
                let value = self
                    .values
                    .get(base_id.index)
                    .expect("missing base value for ssa instance");
                if let SsaValueKind::Base(_) = &value.kind {
                    Ok(base_id)
                } else {
                    panic!("ssa instance's base pointed to another ssa instance");
                }
            }
        }
    }

    fn ssa_base_value(&self, id: ValueId) -> Result<Value<'_, T>, ValueError> {
        let base_id = self.ssa_base_id(id)?;
        Ok(self
            .values
            .get(base_id.index)
            .expect("missing ssa value")
            .try_into()
            .unwrap())
    }

    pub(crate) fn make_unique_ssa_for(
        &mut self,
        id: ValueId,
        #[cfg(feature = "value_locs")] loc: frontend::sourceloc::StaticSourceLoc,
    ) -> Result<ValueId, ValueError> {
        let base_id = self.ssa_base_id(id)?;
        let instance = self.ssa_instances.entry(base_id).or_default().len();

        let base_value = self.value_for_id(base_id)?;

        let instance_val: SsaValue<T> = SsaValue::new(
            SsaValueKind::Instance { base_id, instance },
            base_value.ty.clone(),
            #[cfg(feature = "value_locs")]
            loc,
        );
        Ok(self.insert(instance_val))
    }
}
