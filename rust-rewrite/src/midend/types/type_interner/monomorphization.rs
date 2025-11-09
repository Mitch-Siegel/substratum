use crate::midend::*;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, HashSet};

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

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub enum ParamSubst {
    Concrete(types::Semantic),
    Dependent(GenericParam),
}

#[derive(Debug, Clone, Default, Hash, PartialEq, Eq)]
pub struct ParamSubstMap {
    pub substitutions: BTreeMap<GenericParam, ParamSubst>,
}

impl ParamSubstMap {
    pub fn new(substitutions: Vec<(GenericParam, ParamSubst)>) -> Self {
        Self {
            substitutions: substitutions.into_iter().collect(),
        }
    }

    pub fn empty() -> Self {
        Self::new(Vec::new())
    }

    pub fn is_concrete(&self) -> bool {
        self.substitutions
            .iter()
            .filter_map(|(_, subst)| match subst {
                ParamSubst::Concrete(_) => Some(()),
                ParamSubst::Dependent(_) => None,
            })
            .count()
            == self.substitutions.len()
    }

    // given a set of params, convert this map into a map containing *only* keys for the params, or
    // Err if not all params exist as keys
    pub fn minimal_over_params(
        mut self,
        params: HashSet<GenericParam>,
    ) -> Result<Self, &'static str> {
        self.substitutions = self
            .substitutions
            .into_iter()
            .map(|(k, v)| {
                if params.contains(&k) {
                    Some((k, v))
                } else {
                    None
                }
            })
            .flatten()
            .collect();

        Ok(self)
    }
}

#[derive(Debug)]
pub struct InstanceSet {
    underlying_definition: symtab::TypeDefinition,
    instances: HashSet<ParamSubstMap>,
}

impl InstanceSet {
    pub fn new(underlying_definition: symtab::TypeDefinition) -> Self {
        Self {
            underlying_definition,
            instances: std::iter::once(ParamSubstMap::empty()).collect(),
        }
    }

    pub fn insert(&mut self, params: ParamSubstMap) -> bool {
        self.instances.insert(params)
    }

    pub fn get_underlying(
        &self,
        params: &ParamSubstMap,
    ) -> Result<&symtab::TypeDefinition, String> {
        match self.instances.get(params) {
            Some(_) => Ok(&self.underlying_definition),
            _ => Err("no instance recorded for given params".into()),
        }
    }
}
