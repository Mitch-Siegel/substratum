use crate::midend::{ir::*, *};

pub mod operands;
use operands::*;

#[allow(unused)]
#[enum_delegate::register]
pub trait Lowerable {
    fn lower(self, context: &mut treewalk::LinearizeCtx, loc: SourceLoc);
}

#[derive(Debug, PartialEq, Eq, Clone)]
#[enum_delegate::implement(Lowerable)]
pub enum Operation {
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
    pub fn read_value_ids(&self) -> Vec<ValueId> {
        vec![]
    }

    pub fn write_value_ids(&self) -> Vec<ValueId> {
        vec![]
    }
}

pub fn new_match(scrutinee: ValueId, arms: Vec<MatchArm>) -> Operation {
    Operation::Match(MatchOperands { scrutinee, arms })
}

pub fn new_discriminant(destination: ValueId, enum_receiver: ValueId) -> Operation {
    Operation::Discriminant(DiscriminantOperands {
        destination,
        enum_receiver,
    })
}

pub fn new_get_field_pointer(
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
