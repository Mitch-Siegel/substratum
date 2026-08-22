pub(crate) mod semantic_types;
pub(crate) mod type_interner;

pub(crate) use frontend::types::Syntactic;
pub(crate) use semantic_types::Semantic;
pub(crate) use type_interner::{
    GenericParam, GenericParamsList, Interner, ParamSubst, ParamSubstMap,
};
