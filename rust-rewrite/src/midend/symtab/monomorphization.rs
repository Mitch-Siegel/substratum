use crate::midend::{symtab::*, *};
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};

#[derive(Debug, Hash, PartialEq, Eq)]
enum Param {
    TypeParam(String),
}

#[derive(Debug, Hash, PartialEq, Eq)]
enum ParamSubst {
    Concrete(types::Semantic),
    Dependent(Param),
}

#[derive(Debug, Default, Hash, PartialEq, Eq)]
pub struct ParamSubstMap(BTreeMap<Param, ParamSubst>);
impl ParamSubstMap {
    pub fn is_concrete(&self) -> bool {
        self.0
            .iter()
            .filter_map(|(_, subst)| match subst {
                ParamSubst::Concrete(_) => Some(()),
                ParamSubst::Dependent(_) => None,
            })
            .count()
            == self.0.len()
    }
}

#[derive(Debug, Default, PartialEq, Eq)]
pub struct MonomorphSet {
    instances: HashMap<ParamSubstMap, Option<types::Semantic>>,
}

impl MonomorphSet {
    pub fn new() -> Self {
        Self {
            instances: HashMap::new(),
        }
    }

    pub fn insert(&mut self, params: ParamSubstMap, ty_: Option<types::Semantic>) {
        match self.instances.insert(params, ty_) {
            Some(_existing) => panic!("existing generic monomorphization"),
            None => (),
        }
    }
}

pub struct MonomorphManager {
    monomorphs: HashMap<DefPath, MonomorphSet>,
}

impl MonomorphManager {
    pub fn new() -> Self {
        Self {
            monomorphs: HashMap::new(),
        }
    }

    pub fn add_concrete_monomorph(
        &mut self,
        monomorphized_from: &DefPath,
        params: ParamSubstMap,
        from_definition: &TypeDefinition,
        ty_: types::Semantic,
    ) {
        assert!(params.is_concrete());
        self.monomorphs
            .entry(def_path)
            .or_default()
            .insert(params, Some(ty_));
    }
}
