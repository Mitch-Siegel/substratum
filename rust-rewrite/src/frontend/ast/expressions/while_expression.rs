use crate::frontend::ast::expressions::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
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

impl ValueWalk for WhileExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> midend::ir::ValueId {
        let loop_done_label = context.create_loop(self.loc.clone()).unwrap();

        let condition = self.condition.walk(context);
        let loop_condition_jump = midend::ir::IrLine::new_jump(
            self.loc.clone(),
            loop_done_label,
            midend::ir::JumpCondition::Eq(midend::ir::DualSourceOperands::new(
                condition.into(),
                *context.value_id_for_constant(0),
            )),
        );

        context
            .append_jump_to_current_block(loop_condition_jump)
            .unwrap();

        context
            .unconditional_branch_from_current(self.loc.clone())
            .unwrap();
        self.body.walk(context);
        context.finish_branch().unwrap();

        context.finish_loop(self.loc.clone(), Vec::new()).unwrap();

        context.unit_value_id()
    }
}
