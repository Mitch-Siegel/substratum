use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct FieldExpressionTree {
    pub loc: SourceLoc,
    pub receiver: Expression,
    pub field: String,
}

impl FieldExpressionTree {
    pub fn new(loc: SourceLoc, receiver: Expression, field: String) -> Self {
        Self {
            loc,
            receiver,
            field,
        }
    }
}

impl Display for FieldExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}", self.receiver, self.field)
    }
}

// returns (receiver, field_info)
// receiver is the value id for the receiver of the field access
// field_info is a value id for the field being accessed
impl treewalk::Linearize<(midend::ir::ValueId, String)> for FieldExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> (midend::ir::ValueId, String) {
        let receiver = self.receiver.linearize(ctx);

        (receiver, self.field)
    }
}
