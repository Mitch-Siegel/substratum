use crate::midend::ir::unlowered::*;

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct FieldPointerOperands {
    pub receiver: ValueId,
    pub field_name: String,
    pub destination: ValueId,
}

impl Lowerable for FieldPointerOperands {
    fn lower<P: symtab::Path>(self, _ctx: &mut treewalk::FunctionLinearizeCtx<P>, _loc: SourceLoc) {
        unimplemented!();
    }
}

impl OperandTypeInference for FieldPointerOperands {
    fn infer_types<'a>(&mut self, _ctx: &TypeInferenceContext<'a>) -> bool {
        unimplemented!();
    }
}
