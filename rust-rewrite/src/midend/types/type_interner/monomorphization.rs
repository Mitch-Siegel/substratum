use crate::midend::{symtab::*, *};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};

#[derive(Serialize, Deserialize, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum GenericParam {
    TypeParam(String),
}

impl std::fmt::Display for GenericParam {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TypeParam(t) => write!(f, "{}", t),
        }
    }
}

pub type GenericParamsList = Vec<GenericParam>;

#[derive(Debug, Hash, PartialEq, Eq)]
enum ParamSubst {
    Concrete(types::Semantic),
    Dependent(GenericParam),
}

#[derive(Debug, Default, Hash, PartialEq, Eq)]
pub struct ParamSubstMap(BTreeMap<GenericParam, ParamSubst>);
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
        def_path: DefPath,
        params: ParamSubstMap,
        ty_: types::Semantic,
    ) {
        assert!(params.is_concrete());
        self.monomorphs
            .entry(def_path)
            .or_default()
            .insert(params, Some(ty_));
    }

    pub fn add_dependent_monomorph(&mut self, def_path: DefPath, params: ParamSubstMap) {
        assert!(!params.is_concrete());

        self.monomorphs
            .entry(def_path)
            .or_default()
            .insert(params, None);
    }
}
