use crate::midend::{ir::*, *};

pub(crate) mod operands;
use operands::*;

#[allow(unused)]
#[enum_delegate::register]
pub(crate) trait Lowerable {
    fn lower(self, context: &mut treewalk::FunctionLinearizeCtx, loc: SourceLoc);
}

#[derive(Debug, PartialEq, Eq, Clone)]
#[enum_delegate::implement(Lowerable)]
pub(crate) enum Operation {
    Match(MatchOperands),
    Discriminant(DiscriminantOperands),
    GetFieldPointer(FieldPointerOperands),
}

impl OperandTypeInference for Operation {
    fn infer_types(&mut self, ctx: &TypeInferenceContext) -> bool {
        match self {
            Self::Match(m) => m.infer_types(ctx),
            Self::Discriminant(d) => d.infer_types(ctx),
            Self::GetFieldPointer(f) => f.infer_types(ctx),
        }
    }
}

impl std::fmt::Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Match(_) => write!(f, "match"),
            Self::Discriminant(_) => write!(f, "discriminant"),
            Self::GetFieldPointer(fp) => write!(
                f,
                "field pointer ({} = &{}.{})",
                fp.destination, fp.receiver, fp.field_name
            ),
        }
    }
}

impl Operation {
    pub(crate) fn read_value_ids(&self) -> Vec<ValueId> {
        vec![]
    }

    pub(crate) fn write_value_ids(&self) -> Vec<ValueId> {
        vec![]
    }
}

pub(crate) fn new_match(scrutinee: ValueId, arms: Vec<MatchArm>) -> Operation {
    Operation::Match(MatchOperands { scrutinee, arms })
}

pub(crate) fn new_discriminant(destination: ValueId, enum_receiver: ValueId) -> Operation {
    Operation::Discriminant(DiscriminantOperands {
        destination,
        enum_receiver,
    })
}

pub(crate) fn new_get_field_pointer(
    receiver: ValueId,
    field_name: String,
    destination: ValueId,
) -> Operation {
    Operation::GetFieldPointer(FieldPointerOperands {
        receiver,
        field_name,
        destination,
    })
}
