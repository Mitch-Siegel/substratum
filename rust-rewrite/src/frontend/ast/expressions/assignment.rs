use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct AssignmentTree {
    pub assignee: Box<Expression>,
    pub value: Box<Expression>,
}

impl Ast for AssignmentTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.assignee.loc().merge(&self.value.loc()).unwrap()
    }
}

impl Display for AssignmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.assignee, self.value)
    }
}

impl midend::treewalk::Collect for AssignmentTree {
    fn collect_symbols(
        &self,
        ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        self.value.collect_symbols(ctx)
    }
}

impl midend::treewalk::Linearize for AssignmentTree {
    type Data = midend::ir::ValueId;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        mut ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let assignment_start = self.loc().start();

        let (assignment_ir, mut ctx) = match *self.assignee {
            Expression::FieldExpression(field_expression_tree) => {
                let field_loc = field_expression_tree.loc();
                let (receiver, field);
                ((receiver, field), ctx) = field_expression_tree.linearize_same_path(ctx)?;
                let field_pointer_temp = ctx.function_mut().values_mut().next_temp();

                let field_pointer_line = midend::ir::IrLine::new_get_field_pointer(
                    field_loc.start(),
                    receiver,
                    field,
                    field_pointer_temp,
                );
                ctx.function_mut()
                    .append_statement_to_current_block(field_pointer_line)
                    .unwrap();

                let (stored_value, ctx) = self.value.linearize(ctx)?;

                (
                    midend::ir::IrLine::new_store(
                        assignment_start,
                        stored_value,
                        field_pointer_temp,
                    ),
                    ctx,
                )
            }
            _ => {
                let assignee_start = self.assignee.loc().start();
                let (assignee, ctx) = self.assignee.linearize_same_path(ctx)?;
                let (stored_value, ctx) = self.value.linearize(ctx)?;
                (
                    midend::ir::IrLine::new_assignment(assignee_start, assignee, stored_value),
                    ctx,
                )
            }
        };

        ctx.function_mut()
            .append_statement_to_current_block(assignment_ir)
            .unwrap();

        ctx.into_result(midend::ir::ValueInterner::unit_value_id())
    }
}
