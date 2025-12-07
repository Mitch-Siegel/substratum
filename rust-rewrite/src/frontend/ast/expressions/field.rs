use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct FieldExpressionTree {
    pub receiver: Expression,
    pub field: IdentifierTree,
}

impl Ast<(midend::ir::ValueId, String)> for FieldExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.receiver.loc().merge(&self.field.loc()).unwrap()
    }
}

impl Display for FieldExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}", self.receiver, self.field)
    }
}

impl midend::treewalk::Treewalk<(midend::ir::ValueId, String)> for FieldExpressionTree {
    // returns (receiver, field_info)
    // receiver is the value id for the receiver of the field access
    // field_info is a value id for the field being accessed
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> (midend::ir::ValueId, String) {
        let receiver = self.receiver.linearize(ctx);

        (receiver, self.field.linearize(ctx))
    }
}
