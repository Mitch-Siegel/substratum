use serde::Serialize;

use frontend::loc;

use crate::{
    ir::{
        block_manager::branch,
        unlowered::{Lowerable, ValueId, treewalk},
    },
    types,
};

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct FieldPointerOperands {
    pub receiver: ValueId,
    pub field_name: String,
    pub destination: ValueId,
}

impl Lowerable for FieldPointerOperands {
    fn lower(self, _ctx: &mut treewalk::FunctionLinearizeCtx<impl branch::Branch>, _loc: loc::Loc) {
        unimplemented!();
    }
}

impl types::Inference for FieldPointerOperands {
    fn infer_types(&mut self, _ctx: &mut types::inference::Ctx) -> types::inference::Output {
        unimplemented!();
    }
}
