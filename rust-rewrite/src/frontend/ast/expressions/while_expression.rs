use crate::frontend::ast::expressions::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct WhileExpressionTree {
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

impl midend::treewalk::Collect for WhileExpressionTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx = self.condition.collect_to_ctx(ctx)?;
        self.body.collect_symbols(ctx)
    }
}

impl midend::treewalk::Linearize<midend::ir::ValueId> for WhileExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> midend::ir::ValueId {
        let loc = self.loc();

        let parent_scope_def_path = ctx.def_path().clone();
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
        let condition = self.condition.linearize(ctx);
        let loop_condition_jump = midend::ir::IrLine::new_jump(
            condition_loc.end(),
            loop_done_label,
            midend::ir::lowered::operands::JumpCondition::Conditional(
                midend::ir::lowered::operands::BinaryComparisonOperands::new(
                    condition.into(),
                    *ctx.function_mut().values_mut().id_for_constant(0),
                    midend::ir::lowered::operands::BinaryComparisonKind::EQ,
                ),
            ),
        );

        ctx.function_mut()
            .append_jump_to_current_block(loop_condition_jump)
            .unwrap();

        let parent_def_path = ctx.def_path().clone();
        ctx.function_mut()
            .unconditional_branch_from_current(
                loc.clone().end(),
                parent_def_path.clone(),
                parent_def_path,
            )
            .unwrap();
        self.body.linearize(ctx);

        ctx.function_mut().finish_branch(loc.clone().end()).unwrap();

        ctx.function_mut()
            .finish_loop(loc.end(), Vec::new())
            .unwrap();

        midend::ir::ValueInterner::unit_value_id()
    }
}

impl Display for WhileExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "while ({}) {}", self.condition, self.body)
    }
}
