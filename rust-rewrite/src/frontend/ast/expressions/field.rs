use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct FieldExpressionTree {
    pub receiver: Expression,
    pub field: IdentifierTree,
}

impl Ast for FieldExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.receiver.loc().merge(&self.field.loc()).unwrap()
    }
}

impl Display for FieldExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}", self.receiver, self.field)
    }
}

impl midend::treewalk::Linearize for FieldExpressionTree {
    type Data = (midend::ir::ValueId, String);
    // returns (receiver, field_info)
    // receiver is the value id for the receiver of the field access
    // field_info is a value id for the field being accessed
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let (receiver, ctx) = self.receiver.linearize_same_path(ctx)?;

        let (field_name, ctx) = self.field.linearize(ctx)?;
        ctx.into_result((receiver, field_name))
    }
}
