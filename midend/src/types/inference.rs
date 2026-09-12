use crate::{
    ir::{self, ValueError, ValueId, value},
    symtab, types,
};

pub(crate) trait Inference {
    fn infer_types(&mut self, ctx: &Ctx<'_>) -> bool;
}

#[allow(unused)]
pub(crate) struct Ctx<'a> {
    pub types: &'a types::Interner,
    pub values: &'a mut ir::value::ValueInterner<Option<types::Syntactic>>,
}

impl<'a> Ctx<'a> {
    pub(crate) fn new(
        types: &'a types::Interner,
        values: &'a mut value::ValueInterner<Option<types::Syntactic>>,
    ) -> Self {
        Self { types, values }
    }
}

#[allow(unused)]
pub(crate) enum TypePropagationError {
    ValueError(value::ValueError),
    AlreadyHasType(ValueId, types::Syntactic),
}

impl std::fmt::Display for TypePropagationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::ValueError(ve) => write!(f, "{ve}"),
            Self::AlreadyHasType(v, t) => write!(f, "{v} already has type {t}"),
        }
    }
}

impl From<ValueError> for TypePropagationError {
    fn from(ve: ValueError) -> Self {
        Self::ValueError(ve)
    }
}

impl Ctx<'_> {
    pub(crate) fn _type_for_value(&self, value_id: ValueId) -> Option<&types::Syntactic> {
        match self.values.type_for_id(value_id) {
            Ok(ty) => ty.as_ref(),
            Err(_) => None,
        }
    }

    pub(crate) fn _assign_type_to_value(
        &mut self,
        value_id: ValueId,
        ty: types::Syntactic,
    ) -> Result<(), TypePropagationError> {
        let value = self.values.value_mut_for_id(value_id)?;
        match value.ty_mut().replace(ty) {
            None => Ok(()),
            Some(existing) => Err(TypePropagationError::AlreadyHasType(value_id, existing)),
        }
    }
}

fn infer_types_for_function(_f: &symtab::values::Function, _type_interner: &types::Interner) {}

pub(super) fn on_symbol(
    _path: &symtab::RawPath,
    symbol: &symtab::SymbolDef,
    type_interner: &mut types::Interner,
) {
    if let symtab::SymbolDef::Value(symtab::Value::Function(f)) = symbol {
        infer_types_for_function(f, type_interner);
    }
}
