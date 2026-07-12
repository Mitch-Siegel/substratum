use crate::{
    frontend::ast::expressions::*,
    midend::{
        symtab::ValuePath,
        treewalk::{PathedCtxTrait, ValueFunctionLinearizeCtx},
    },
};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct WhileExpressionTree {
    pub while_keyword_loc: sourceloc::SourceSpan,
    pub condition: Expression,
    pub body: BlockExpressionTree,
}

impl Ast for WhileExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.while_keyword_loc
            .clone()
            .merge(&self.body.loc())
            .unwrap()
    }
}

impl midend::treewalk::Collect<ValuePath> for WhileExpressionTree {
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx = self.condition.collect_symbols(ctx)?;
        self.body.collect_symbols(ctx)?.into_result()
    }
}

impl
    midend::treewalk::Linearize<
        midend::treewalk::UnpathedFunctionLinearizeCtx,
        ValuePath,
        ValueFunctionLinearizeCtx,
    > for WhileExpressionTree
{
    type Data = midend::ir::ValueId;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: ValueFunctionLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data, midend::treewalk::UnpathedFunctionLinearizeCtx>
    {
        let loc = self.loc();

        let parent_scope_def_path = ctx.path().clone();
        let loop_scope_def_path = ctx.reserve_subscope();
        let loop_done_label = ctx
            .function_mut()
            .create_loop(
                loc.clone().start(),
                parent_scope_def_path,
                loop_scope_def_path,
            )
            .unwrap();

        let condition_loc = self.condition.loc();
        let condition;
        (condition, ctx) = self.condition.linearize(ctx)?;
        let loop_condition_jump = midend::ir::IrLine::new_jump(
            condition_loc.end(),
            loop_done_label,
            midend::ir::lowered::operands::JumpCondition::Conditional(
                midend::ir::lowered::operands::BinaryComparisonOperands::new(
                    condition,
                    *ctx.function_mut().values_mut().id_for_constant(0),
                    midend::ir::lowered::operands::BinaryComparisonKind::EQ,
                ),
            ),
        );

        ctx.function_mut()
            .append_jump_to_current_block(loop_condition_jump)
            .unwrap();

        let parent_def_path = ctx.path().clone();
        ctx.function_mut()
            .unconditional_branch_from_current(
                loc.clone().end(),
                parent_def_path.clone(),
                parent_def_path,
            )
            .unwrap();
        let (_, mut ctx) = self.body.linearize(ctx)?;

        ctx.function_mut().finish_branch(loc.clone().end()).unwrap();

        ctx.function_mut()
            .finish_loop(loc.end(), Vec::new())
            .unwrap();

        ctx.into_result(midend::ir::ValueInterner::unit_value_id())
    }
}

impl Display for WhileExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "while ({}) {}", self.condition, self.body)
    }
}
