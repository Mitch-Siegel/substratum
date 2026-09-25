use std::{collections::HashMap, ops};

use crate::{
    ir::{self, ValueError, ValueId, value},
    symtab::{self, Symtab},
    types::{self},
};

#[derive(Debug)]
pub(crate) enum BreakReason {
    AlreadyKnown,
    #[allow(unused)] // FUTURE: use this to build dependency graph for type inference
    Stalled(ValueId),
}

#[derive(Debug)]
pub(crate) enum ContinueReason {
    #[allow(unused)] // FUTURE: use this to build dependency graph for type inference
    Inferred(ValueId),
    AlreadyDone,
}

pub(crate) type Output = ops::ControlFlow<BreakReason, ContinueReason>;

/// Single-line-level IR inference trait
pub(crate) trait Inference {
    /// Returns none if types have been fully inferred, or value ID being
    /// waited on for additional type information if there is more work to do
    fn infer_types(&mut self, ctx: &mut Ctx<'_>) -> Output;
}

#[allow(unused)]
pub(crate) struct Ctx<'a> {
    pub symtab: &'a symtab::SymbolTable,
    pub types: &'a types::Interner,
    pub values: &'a mut ir::value::ValueInterner<Option<types::Syntactic>>,
}

impl<'a> Ctx<'a> {
    pub(crate) fn new(
        symtab: &'a symtab::SymbolTable,
        types: &'a types::Interner,
        values: &'a mut value::ValueInterner<Option<types::Syntactic>>,
    ) -> Self {
        Self {
            symtab,
            types,
            values,
        }
    }
}

#[derive(Debug)]
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
    pub(crate) fn check_inferee_type(
        &self,
        value_id: ValueId,
    ) -> ops::ControlFlow<BreakReason, ()> {
        match self.values.type_for_id(value_id).unwrap() {
            Some(_ty) => ops::ControlFlow::Break(BreakReason::AlreadyKnown),
            None => ops::ControlFlow::Continue(()),
        }
    }

    pub(crate) fn type_for_value(
        &self,
        value_id: ValueId,
    ) -> ops::ControlFlow<BreakReason, &types::Syntactic> {
        let maybe_type = self.values.type_for_id(value_id).unwrap();
        match maybe_type {
            Some(ty) => ops::ControlFlow::Continue(ty),
            None => ops::ControlFlow::Break(BreakReason::Stalled(value_id)),
        }
    }

    pub(crate) fn assign_type_to_inferee(
        &mut self,
        value_id: ValueId,
        ty: types::Syntactic,
    ) -> Result<Output, TypePropagationError> {
        let value = self.values.value_mut_for_id(value_id)?;
        // TODO: fix up API here
        match value.ty.replace(ty) {
            None => Ok(Output::Continue(ContinueReason::Inferred(value_id))),
            Some(existing) => Err(TypePropagationError::AlreadyHasType(value_id, existing)),
        }
    }
}

fn infer_types_for_function(
    symtab: &symtab::SymbolTable,
    f: &symtab::values::Function,
    type_interner: &types::Interner,
) {
    let Some(cf) = &mut *f.control_flow.borrow_mut() else {
        return;
    };

    let mut assign_types = HashMap::new();

    for id in cf.values().ids() {
        let value = cf.values().value_for_id(id).unwrap();
        if value.ty.is_some() {
            continue;
        }

        match cf.values().kind_of(id).unwrap() {
            ir::ValueKind::Argument(idx) => {
                assign_types.insert(id, f.prototype.arguments[*idx].type_().unwrap().clone());
            }
            ir::ValueKind::Variable(path) => {
                let val_ty = symtab.lookup_value_at(path).unwrap();
                assign_types.insert(id, val_ty.syntactic());
            }
            ir::ValueKind::StaticFunction(_) => unimplemented!(),
            ir::ValueKind::Constant(_) | ir::ValueKind::Temporary(_) => (),
        }
    }

    for (val, ty) in assign_types {
        cf.values_mut().assign_type_to_id(val, ty).unwrap();
    }

    cf.infer_types(symtab, type_interner);
}

pub(super) fn on_symbol(
    symtab: &symtab::SymbolTable,
    _path: &symtab::RawPath,
    symbol: &symtab::SymbolDef,
    type_interner: &mut types::Interner,
) {
    if let symtab::SymbolDef::Value(symtab::Value::Function(f)) = symbol {
        infer_types_for_function(symtab, f, type_interner);
    }
}
