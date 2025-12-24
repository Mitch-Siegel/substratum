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

impl std::fmt::Display for ParamSubst {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Concrete(semantic) => write!(f, "{}", semantic),
            Self::Dependent(dependent) => write!(f, "{}", dependent),
        }
    }
}

#[derive(Clone, Default, Hash, PartialEq, Eq)]
pub struct ParamSubstMap {
    pub substitutions: BTreeMap<GenericParam, ParamSubst>,
}

impl ParamSubstMap {
    pub fn new(substitutions: impl Iterator<Item = (GenericParam, ParamSubst)>) -> Self {
        Self {
            substitutions: substitutions.collect(),
        }
    }

    pub fn empty() -> Self {
        Self {
            substitutions: BTreeMap::new(),
        }
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

    pub fn substitutions_in_order(
        &self,
        order: &Vec<GenericParam>,
    ) -> Result<Vec<&ParamSubst>, String> {
        if order.len() != self.substitutions.len() {
            return Err(format!("order provided to substitutions_in_order() has {} params, but subst map has {} kvps", order.len(), self.substitutions.len()));
        }
        let mut substs = Vec::with_capacity(order.len());

        for param in order {
            match self.substitutions.get(&param) {
                Some(subst) => substs.push(subst),
                None => return Err(format!("param{} not present", param)),
            }
        }

        Ok(substs)
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

impl std::fmt::Display for ParamSubstMap {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<")?;
        let mut first = true;
        for (k, v) in &self.substitutions {
            if !first {
                write!(f, ", {}={}", k, v)?;
            } else {
                write!(f, "{}={}", k, v)?;
                first = false
            }
        }
        write!(f, ">")
    }
}

impl std::fmt::Debug for ParamSubstMap {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<")?;
        let mut first = true;
        for (k, v) in &self.substitutions {
            if !first {
                write!(f, ", {:?}={:?}", k, v)?;
            } else {
                write!(f, "{:?}={:?}", k, v)?;
                first = false
            }
        }
        write!(f, ">")
    }
}

#[derive(Debug)]
pub struct InstanceSet {
    underlying_definition: symtab::TypeDecl,
    instances: HashSet<ParamSubstMap>,
}

impl InstanceSet {
    pub fn new(underlying_definition: symtab::TypeDecl) -> Self {
        // special case for non-generic types. We must still be able to call get_underlying, which
        // requires lookup to succeed (only) when an empty substitution map is passed
        let instances = if underlying_definition.generic_params().len() == 0 {
            std::iter::once(ParamSubstMap::empty()).collect()
        } else {
            HashSet::new()
        };
        Self {
            underlying_definition,
            instances,
        }
    }

    pub fn insert(&mut self, params: ParamSubstMap) -> bool {
        self.instances.insert(params)
    }

    pub fn get_underlying(&self, params: &ParamSubstMap) -> Result<&symtab::TypeDecl, String> {
        trace::trace!(
            "get underlying type definition {} for generic params {}",
            self.underlying_definition.syntactic(),
            params
        );
        match self.instances.get(params) {
            Some(_) => Ok(&self.underlying_definition),
            _ => Err("no instance recorded for given params".into()),
        }
    }

    pub fn instance_iter(&self) -> impl Iterator<Item = &ParamSubstMap> {
        self.instances.iter()
    }
}
