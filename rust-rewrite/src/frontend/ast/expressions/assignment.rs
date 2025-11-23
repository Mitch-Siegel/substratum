use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct AssignmentTree {
    pub loc: SourceLoc,
    pub assignee: Box<Expression>,
    pub value: Box<Expression>,
}
impl AssignmentTree {
    pub fn new(loc: SourceLoc, assignee: Expression, value: Expression) -> Self {
        Self {
            loc,
            assignee: Box::from(assignee),
            value: Box::from(value),
        }
    }
}
impl Display for AssignmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.assignee, self.value)
    }
}

impl treewalk::Linearize<midend::ir::ValueId> for AssignmentTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::ir::ValueId {
        let assignment_ir = match *self.assignee {
            Expression::FieldExpression(field_expression_tree) => {
                let field_loc = field_expression_tree.loc.clone();
                let (receiver, field) = field_expression_tree.linearize(ctx);
                let field_pointer_temp = ctx.function_mut().values_mut().next_temp();

                let field_pointer_line = midend::ir::IrLine::new_get_field_pointer(
                    field_loc,
                    receiver,
                    field,
                    field_pointer_temp,
                );
                ctx.function_mut()
                    .append_statement_to_current_block(field_pointer_line)
                    .unwrap();

                midend::ir::IrLine::new_store(
                    self.loc,
                    self.value.linearize(ctx).into(),
                    field_pointer_temp,
                )
            }
            _ => midend::ir::IrLine::new_assignment(
                self.loc,
                self.assignee.linearize(ctx).into(),
                self.value.linearize(ctx).into(),
            ),
        };

        ctx.function_mut()
            .append_statement_to_current_block(assignment_ir)
            .unwrap();

        midend::ir::ValueInterner::unit_value_id()
    }
}
