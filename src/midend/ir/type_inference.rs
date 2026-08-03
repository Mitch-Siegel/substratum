use crate::midend::ir::{symtab, types, value, ValueError, ValueId, ValueInterner};

pub(crate) trait OperandTypeInference {
    fn infer_types(&mut self, ctx: &TypeInferenceContext) -> bool;
}

pub(crate) struct TypeInferenceContext<'a> {
    pub symtab: &'a mut symtab::SymbolTable,
    pub values: &'a mut ValueInterner,
}

impl<'a> TypeInferenceContext<'a> {
    pub(crate) fn new(symtab: &'a mut symtab::SymbolTable, values: &'a mut ValueInterner) -> Self {
        Self { symtab, values }
    }
}

#[allow(unused)]
pub(crate) enum TypePropagationError {
    ValueError(value::ValueError),
}

impl std::fmt::Display for TypePropagationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::ValueError(ve) => write!(f, "{ve}"),
        }
    }
}

impl From<ValueError> for TypePropagationError {
    fn from(ve: ValueError) -> Self {
        Self::ValueError(ve)
    }
}

impl TypeInferenceContext<'_> {
    pub(crate) fn _type_for_value(&self, value_id: ValueId) -> Option<types::Semantic> {
        self.values.semantic_for_id(value_id).ok()
    }

    pub(crate) fn _assign_type_to_value(
        &mut self,
        value_id: ValueId,
        ty: types::Semantic,
    ) -> Result<(), TypePropagationError> {
        let value = self.values.value_mut_for_id(value_id)?;
        value.set_type(ty)?;
        Ok(())
    }
}
