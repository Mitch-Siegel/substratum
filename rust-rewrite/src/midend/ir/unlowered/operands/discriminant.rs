use crate::midend::ir::unlowered::*;

// get the discriminant value of an enum
#[derive(Debug, PartialEq, Eq, Clone)]
pub(crate) struct DiscriminantOperands {
    pub destination: ValueId,
    pub(crate) enum_receiver: ValueId,
}

impl Lowerable for DiscriminantOperands {
    fn lower(self, ctx: &mut treewalk::FunctionLinearizeCtx, loc: SourceLoc) {
        // sanity check
        let _receiver_type = ctx
            .function_mut()
            .values_mut()
            .value_for_id(&self.enum_receiver)
            .unwrap();
        // TODO: type propagation and checking to verify this thing is actually an enum

        let _discriminant_line = ir::IrLine::new_load(loc, self.enum_receiver, self.destination);
    }
}

impl OperandTypeInference for DiscriminantOperands {
    fn infer_types<'a>(&mut self, _ctx: &TypeInferenceContext<'a>) -> bool {
        unimplemented!();
    }
}
