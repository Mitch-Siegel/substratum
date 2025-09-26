use crate::midend::ir::unlowered::*;

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct FieldPointerOperands {
    pub receiver: ValueId,
    pub field_name: String,
    pub destination: ValueId,
}

impl Lowerable for FieldPointerOperands {
    fn lower(self, _ctx: &mut linearizer::WalkContext, _loc: SourceLoc) {
        unimplemented!();
    }
}

impl OperandTypePropagation for FieldPointerOperands {
    fn propagate_types<'a>(&self, _ctx: &TypePropagationContext<'a>) -> bool {
        unimplemented!();
    }
}
