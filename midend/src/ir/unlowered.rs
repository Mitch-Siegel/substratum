use frontend::sourceloc;

use crate::{
    ir::{OperandTypeInference, Serialize, TypeInferenceContext, ValueId},
    treewalk,
};

pub(crate) mod operands;
use operands::{DiscriminantOperands, FieldPointerOperands, MatchArm, MatchOperands};

#[allow(unused)]
pub(crate) trait Lowerable {
    fn lower(self, context: &mut treewalk::FunctionLinearizeCtx, loc: sourceloc::SourceLoc);
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub(crate) enum Operation {
    Match(MatchOperands),
    Discriminant(DiscriminantOperands),
    GetFieldPointer(FieldPointerOperands),
}

impl Lowerable for Operation {
    fn lower(self, context: &mut treewalk::FunctionLinearizeCtx, loc: sourceloc::SourceLoc) {
        match self {
            Self::Match(m) => m.lower(context, loc),
            Self::Discriminant(d) => d.lower(context, loc),
            Self::GetFieldPointer(gfp) => gfp.lower(context, loc),
        }
    }
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
