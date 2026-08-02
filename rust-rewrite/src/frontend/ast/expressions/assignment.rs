use crate::{frontend::ast::*, midend::symtab::ValuePath};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct AssignmentTree {
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

impl midend::treewalk::Collect<ValuePath> for AssignmentTree {
    fn collect_inner(
        &self,
        ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        self.value.collect_inner(ctx)
    }
}

impl
    midend::treewalk::Linearize<
        midend::treewalk::UnpathedFunctionLinearizeCtx,
        midend::symtab::ScopePath,
        treewalk::FunctionLinearizeCtx,
    > for AssignmentTree
{
    type Data = midend::ir::ValueId;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: treewalk::FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, midend::treewalk::UnpathedFunctionLinearizeCtx> {
        let assignment_start = self.loc().start();

        let (assignment_ir, mut ctx) = match *self.assignee {
            Expression::Field(field_expression_tree) => {
                let field_loc = field_expression_tree.loc();
                let (receiver, field);
                ((receiver, field), ctx) = field_expression_tree.linearize(ctx)?;
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
                let (assignee, ctx) = self.assignee.linearize(ctx)?;
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
