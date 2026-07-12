use crate::midend::ir::*;

#[enum_delegate::register]
pub(crate) trait OperandTypeInference {
    fn infer_types(&mut self, ctx: &TypeInferenceContext) -> bool;
}

pub(crate) struct TypeInferenceContext<'a> {
    pub _symtab: &'a mut symtab::SymbolTable,
    pub _values: &'a mut ValueInterner,
}

impl<'a> TypeInferenceContext<'a> {
    pub(crate) fn new(
        _symtab: &'a mut symtab::SymbolTable,
        _values: &'a mut ValueInterner,
    ) -> Self {
        Self { _symtab, _values }
    }
}

#[allow(unused)]
pub(crate) enum TypePropagationError {
    ValueError(value::ValueError),
}

impl std::fmt::Display for TypePropagationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::ValueError(ve) => write!(f, "{}", ve),
        }
    }
}

impl From<ValueError> for TypePropagationError {
    fn from(ve: ValueError) -> Self {
        Self::ValueError(ve)
    }
}

impl<'a> TypeInferenceContext<'a> {
    pub(crate) fn _type_for_value(&self, value_id: &ValueId) -> Option<types::Semantic> {
        self._values.semantic_for_id(value_id).ok()
    }

    pub(crate) fn _assign_type_to_value(
        &mut self,
        value_id: &ValueId,
        ty: types::Semantic,
    ) -> Result<(), TypePropagationError> {
        let value = self._values.value_mut_for_id(value_id)?;
        value.set_type(ty)?;
        Ok(())
    }
}
