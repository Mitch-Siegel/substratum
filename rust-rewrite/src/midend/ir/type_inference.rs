use crate::midend::ir::*;

#[enum_delegate::register]
pub trait OperandTypeInference {
    fn infer_types(&self, ctx: &TypeInferenceContext) -> bool;
}

pub struct TypeInferenceContext<'a> {
    symtab: Box<symtab::SymbolTable>,
    values: &'a mut ValueInterner,
    def_path: symtab::DefPath,
}

impl<'a> TypeInferenceContext<'a> {
    pub fn new(
        symtab: Box<symtab::SymbolTable>,
        values: &'a mut ValueInterner,
        def_path: symtab::DefPath,
    ) -> Self {
        Self {
            symtab,
            values,
            def_path,
        }
    }

    pub fn take(self) -> Box<symtab::SymbolTable> {
        self.symtab
    }
}

pub enum TypePropagationError {
    ValueError(value::ValueError),
}

impl From<ValueError> for TypePropagationError {
    fn from(ve: ValueError) -> Self {
        Self::ValueError(ve)
    }
}

impl<'a> TypeInferenceContext<'a> {
    pub fn type_for_value(&self, value_id: &ValueId) -> Option<types::Semantic> {
        match self.values.semantic_for_id(value_id) {
            Ok(ty) => Some(ty),
            _ => None,
        }
    }

    pub fn assign_type_to_value(
        &mut self,
        value_id: &ValueId,
        ty: types::Semantic,
    ) -> Result<(), TypePropagationError> {
        let value = self.values.value_mut_for_id(value_id)?;
        value.set_type(ty)?;
        Ok(())
    }
}
