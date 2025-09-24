use crate::midend::ir::unlowered::*;

// get the discriminant value of an enum
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct DiscriminantOperands {
    pub destination: ValueId,
    pub enum_receiver: ValueId,
}

impl Lowerable for DiscriminantOperands {
    fn lower<'a>(self, context: &'a mut linearizer::FunctionWalkContext, loc: SourceLoc) {
        // sanity check
        let receiver_type = context
            .values_mut()
            .value_for_id(&self.enum_receiver)
            .unwrap();
        // TODO: type propagation and checking to verify this thing is actually an enum

        let discriminant_line = ir::IrLine::new_load(loc, self.enum_receiver, self.destination);
    }
}

impl OperandTypePropagation for DiscriminantOperands {
    fn propagate_types<'a>(&self, ctx: &TypePropagationContext<'a>) -> bool {
        unimplemented!();
        true
    }
}
