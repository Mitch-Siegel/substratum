use crate::frontend::ast::expressions::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct IfExpressionTree {
    pub if_keyword_loc: sourceloc::SourceSpan,
    pub condition: Expression,
    pub true_block: BlockExpressionTree,
    pub false_block: Option<BlockExpressionTree>,
}

impl Ast for IfExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut loc_span = self
            .if_keyword_loc
            .clone()
            .merge(&self.true_block.loc())
            .unwrap();

        if let Some(false_block) = &self.false_block {
            loc_span = loc_span.merge(&false_block.loc()).unwrap()
        }

        loc_span
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

impl treewalk::Linearize<midend::ir::ValueId> for IfExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::ir::ValueId {
        // FUTURE: optimize condition walk to use different jumps
        let condition_loc = self.condition.loc();
        let if_loc = self.loc();
        let condition_result: midend::ir::ValueId = self.condition.linearize(ctx).into();
        let if_condition = midend::ir::lowered::operands::JumpCondition::Conditional(
            midend::ir::lowered::operands::BinaryComparisonOperands::new(
                condition_result,
                *ctx.function_mut().values_mut().id_for_constant(0),
                midend::ir::lowered::operands::BinaryComparisonKind::NE,
            ),
        );

        let parent_scope_def_path = ctx.def_path().clone();
        let true_scope_def_path = ctx.reserve_subscope();
        let false_scope_def_path = ctx.reserve_subscope();

        ctx.function_mut()
            .conditional_branch_from_current(
                condition_loc.clone().start(),
                if_condition,
                parent_scope_def_path,
                true_scope_def_path,
                false_scope_def_path,
            )
            .unwrap();

        let true_loc = self.true_block.loc();
        let if_value_id = self.true_block.linearize(ctx);

        // create a separate, mutable value which contains the true result
        let result_value = if_value_id.clone();

        // if a false block exists AND the 'if' value exists
        if self.false_block.is_some() {
            // we need to copy the 'if' result to the common result_value at the end of the 'if' block
            let result_value = ctx.function_mut().values_mut().next_temp();
            let assign_if_result_line =
                midend::ir::IrLine::new_assignment(true_loc.start(), result_value, if_value_id);
            ctx.function_mut()
                .append_statement_to_current_block(assign_if_result_line)
                .unwrap();
        }

        ctx.function_mut()
            .finish_true_branch_switch_to_false(condition_loc.start())
            .unwrap();

        // handle branch linearization and assignment to the result value
        match self.false_block {
            Some(else_block) => {
                let else_loc = else_block.loc();
                let else_value_id = else_block.linearize(ctx);

                // if the 'else' value exists (have already passed check to assert types are the same)
                // copy the 'else' result to the common result_value at the end of the 'else' block
                let assign_else_result_line = midend::ir::IrLine::new_assignment(
                    else_loc.end(),
                    result_value.clone().into(),
                    else_value_id,
                );
                ctx.function_mut()
                    .append_statement_to_current_block(assign_else_result_line)
                    .unwrap();
            }
            None => {}
        };

        ctx.function_mut().finish_branch(if_loc.end()).unwrap();

        result_value
    }
}
