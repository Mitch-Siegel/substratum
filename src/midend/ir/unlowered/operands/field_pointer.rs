use crate::midend::ir::unlowered::{
    treewalk, Lowerable, OperandTypeInference, Serialize, SourceLoc, TypeInferenceContext, ValueId,
};

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct FieldPointerOperands {
    pub receiver: ValueId,
    pub field_name: String,
    pub destination: ValueId,
}

impl Lowerable for FieldPointerOperands {
    fn lower(self, _ctx: &mut treewalk::FunctionLinearizeCtx, _loc: SourceLoc) {
        unimplemented!();
    }
}

impl OperandTypeInference for FieldPointerOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext<'_>) -> bool {
        unimplemented!();
    }
}
