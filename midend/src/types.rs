use crate::symtab;

pub(crate) mod inference;
pub(crate) mod semantic_types;
pub(crate) mod type_interner;

pub(crate) use frontend::types::Syntactic;
pub(crate) use inference::Inference;
pub(crate) use semantic_types::Semantic;
pub(crate) use type_interner::{GenericParam, GenericParamsList, Interner, ParamSubst};

pub(crate) fn infer_types(symtab: &symtab::SymbolTable, types: Interner) -> Interner {
    symtab::Visitor::visit_with_starting_data(symtab, inference::on_symbol, types)
}
