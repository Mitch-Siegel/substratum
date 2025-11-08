use crate::midend::{types::*, *};
use std::collections::HashMap;

pub mod monomorphization;
pub mod semantic_function;

pub use monomorphization::*;
pub use semantic_function::*;

#[derive(Clone, Hash, PartialEq, Eq)]
struct DefPathWithParamSubsts {
    pub def_path: symtab::DefPath,
    pub param_substs: ParamSubstMap,
}

impl DefPathWithParamSubsts {
    pub fn new(def_path: symtab::DefPath, param_substs: ParamSubstMap) -> Self {
        Self {
            def_path,
            param_substs,
        }
    }
}

pub struct Interner {
    id_mappings: HashMap<Semantic, DefPathWithParamSubsts>,
    reverse_id_mappings: HashMap<DefPathWithParamSubsts, Semantic>,
    generic_instances: HashMap<symtab::DefPath, monomorphization::InstanceSet>,
}

impl Interner {
    pub fn new() -> Self {
        Self {
            id_mappings: HashMap::new(),
            reverse_id_mappings: HashMap::new(),
            generic_instances: HashMap::new(),
        }
    }

    fn next_id(&self) -> Semantic {
        Semantic {
            id: self.id_mappings.len(),
        }
    }

    pub fn insert_type(
        &mut self,
        def_path: symtab::DefPath,
        definition: symtab::TypeDefinition,
    ) -> Result<Semantic, symtab::SymbolError> {
        assert!(&def_path.is_type());

        let next_id = self.next_id();
        // Ensure that we never overwrite any type
        let no_subst = DefPathWithParamSubsts::new(def_path.clone(), ParamSubstMap::empty());
        let overwritten_id = self.id_mappings.insert(next_id, no_subst.clone());
        match overwritten_id {
            Some(_) => return Err(symtab::SymbolError::AlreadyDefined(def_path.clone())),
            None => next_id,
        };

        assert!(self.reverse_id_mappings.insert(no_subst, next_id).is_none());

        self.generic_instances
            .insert(def_path, InstanceSet::new(definition));

        Ok(next_id)
    }

    pub fn semantic_for_defpath(
        &self,
        def_path: symtab::DefPath,
        param_substs: ParamSubstMap,
    ) -> Option<Semantic> {
        self.reverse_id_mappings
            .get(&DefPathWithParamSubsts::new(def_path, param_substs))
            .cloned()
    }

    pub fn get_type_definition(&self, id: &Semantic) -> Result<&symtab::TypeDefinition, String> {
        let path_with_params = self.id_mappings.get(id).ok_or("no type mapping for ID")?;
        let instance_set = self
            .generic_instances
            .get(&path_with_params.def_path)
            .ok_or(format!(
                "no instance set exists for type {} (id {}) and params {:?}",
                path_with_params.def_path.last(),
                id,
                path_with_params.param_substs
            ))?;
        instance_set.get_underlying(&path_with_params.param_substs)
    }

    pub fn get_syntactic(&self, id: &Semantic) -> Result<&Syntactic, String> {
        Ok(self.get_type_definition(id)?.syntactic())
    }
}
