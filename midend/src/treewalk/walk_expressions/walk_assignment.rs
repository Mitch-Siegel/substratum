use frontend::ast::{self, Ast};

use crate::{
    symtab,
    treewalk::{
        Collect, CollectResult, FunctionLinearizeCtx, Linearize, LinearizeResult,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx, ir,
    },
};

impl Collect<symtab::ValuePath> for ast::expressions::AssignmentTree {
    fn collect_inner(&self, ctx: ValueCollectCtx) -> CollectResult {
        self.value.collect_inner(ctx)
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::AssignmentTree
{
    type Data = ir::ValueId;
    fn linearize_inner(
        self,
        mut ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let assignment_start = self.loc().start();

        let (assignment_ir, mut ctx) =
            if let ast::Expression::Field(field_expression_tree) = *self.assignee {
                let field_loc = field_expression_tree.loc();
                let (receiver, field);
                ((receiver, field), ctx) = field_expression_tree.linearize(ctx)?;
                let field_pointer_temp = ctx.values_mut().next_temp();

                let field_pointer_line = ir::IrLine::new_get_field_pointer(
                    field_loc.start(),
                    receiver,
                    field,
                    field_pointer_temp,
                );
                ctx.append_statement_to_current_block(field_pointer_line);

                let (stored_value, ctx) = self.value.linearize(ctx)?;

                (
                    ir::IrLine::new_store(assignment_start, stored_value, field_pointer_temp),
                    ctx,
                )
            } else {
                let assignee_start = self.assignee.loc().start();
                let (assignee, ctx) = self.assignee.linearize(ctx)?;
                let (stored_value, ctx) = self.value.linearize(ctx)?;
                (
                    ir::IrLine::new_assignment(assignee_start, assignee, stored_value),
                    ctx,
                )
            };

        ctx.append_statement_to_current_block(assignment_ir);

        ctx.into_result(ir::ValueInterner::unit_value_id())
    }
}
