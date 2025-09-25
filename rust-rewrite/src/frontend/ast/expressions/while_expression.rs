use crate::frontend::ast::expressions::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct WhileExpressionTree {
    pub loc: SourceLoc,
    pub condition: ExpressionTree,
    pub body: BlockExpressionTree,
}

impl WhileExpressionTree {
    pub fn new(loc: SourceLoc, condition: ExpressionTree, body: BlockExpressionTree) -> Self {
        Self {
            loc,
            condition,
            body,
        }
    }
}

impl Display for WhileExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "while ({}) {}", self.condition, self.body)
    }
}

impl Walk<midend::ir::ValueId> for WhileExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, ctx: &mut midend::linearizer::WalkContext) -> midend::ir::ValueId {
        let parent_scope_def_path = ctx.def_path().clone();
        let loop_scope_def_path = ctx.reserve_subscope();
        let loop_done_label = ctx
            .function()
            .create_loop(self.loc.clone(), parent_scope_def_path, loop_scope_def_path)
            .unwrap();

        let condition = self.condition.walk(ctx);
        let loop_condition_jump = midend::ir::IrLine::new_jump(
            self.loc.clone(),
            loop_done_label,
            midend::ir::lowered::operands::JumpCondition::Conditional(
                midend::ir::lowered::operands::BinaryComparisonOperands::new(
                    condition.into(),
                    *ctx.function().values_mut().id_for_constant(0),
                    midend::ir::lowered::operands::BinaryComparisonKind::EQ,
                ),
            ),
        );

        ctx.function()
            .append_jump_to_current_block(loop_condition_jump)
            .unwrap();

        let parent_def_path = ctx.def_path().clone();
        ctx.function()
            .unconditional_branch_from_current(
                self.loc.clone(),
                parent_def_path.clone(),
                parent_def_path,
            )
            .unwrap();
        self.body.walk(ctx);
        ctx.function().finish_branch(self.loc.clone()).unwrap();

        ctx.function()
            .finish_loop(self.loc.clone(), Vec::new())
            .unwrap();

        midend::ir::ValueInterner::unit_value_id()
    }
}
