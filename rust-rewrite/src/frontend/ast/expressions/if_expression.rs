use crate::frontend::ast::expressions::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct IfExpressionTree {
    pub loc: SourceLoc,
    pub condition: ExpressionTree,
    pub true_block: BlockExpressionTree,
    pub false_block: Option<BlockExpressionTree>,
}

impl IfExpressionTree {
    pub fn new(
        loc: SourceLoc,
        condition: ExpressionTree,
        true_block: BlockExpressionTree,
        false_block: Option<BlockExpressionTree>,
    ) -> Self {
        Self {
            loc,
            condition,
            true_block,
            false_block,
        }
    }
}

impl Display for IfExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.false_block {
            Some(false_block) => write!(
                f,
                "if {}\n\t{{{}}} else {{{}}}",
                self.condition, self.true_block, false_block
            ),
            None => write!(f, "if {}\n\t{{{}}}", self.condition, self.true_block),
        }
    }
}

impl ValueWalk for IfExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> midend::ir::ValueId {
        // FUTURE: optimize condition walk to use different jumps
        let condition_loc = self.condition.loc.clone();
        let condition_result: midend::ir::ValueId = self.condition.walk(context).into();
        let if_condition =
            midend::ir::JumpCondition::NE(midend::ir::operands::DualSourceOperands::new(
                condition_result,
                *context.value_id_for_constant(0),
            ));

        context
            .conditional_branch_from_current(condition_loc.clone(), if_condition)
            .unwrap();
        let if_value_id = self.true_block.walk(context);

        // create a separate, mutable value which contains the true result
        let result_value = if_value_id.clone();

        // if a false block exists AND the 'if' value exists
        if self.false_block.is_some() {
            // we need to copy the 'if' result to the common result_value at the end of the 'if' block
            let result_value_type = context.type_for_value_id(&result_value).clone();
            let result_value = context.next_temp(result_value_type);
            let assign_if_result_line =
                midend::ir::IrLine::new_assignment(self.loc.clone(), result_value, if_value_id);
            context
                .append_statement_to_current_block(assign_if_result_line)
                .unwrap();
        }

        context.finish_true_branch_switch_to_false().unwrap();

        // handle branch linearization and assignment to the result value
        match self.false_block {
            Some(else_block) => {
                let else_value_id = else_block.walk(context);

                // sanity-check that both branches return the same type
                let if_type_id = context.type_id_for_value_id(&if_value_id);
                let else_type_id = context.type_id_for_value_id(&else_value_id);
                if if_type_id != else_type_id {
                    panic!(
                        "If and Else branches return different types ({:?} and {:?}): {}",
                        if_type_id, else_type_id, self.loc
                    );
                }

                // if the 'else' value exists (have already passed check to assert types are the same)
                // copy the 'else' result to the common result_value at the end of the 'else' block
                let assign_else_result_line = midend::ir::IrLine::new_assignment(
                    self.loc.clone(),
                    result_value.clone().into(),
                    else_value_id,
                );
                context
                    .append_statement_to_current_block(assign_else_result_line)
                    .unwrap();
            }
            None => {}
        };

        context.finish_branch().unwrap();

        result_value
    }
}
