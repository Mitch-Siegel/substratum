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

impl midend::treewalk::Collect<midend::symtab::ValuePath> for IfExpressionTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx = self.true_block.collect_in_place(ctx)?;
        if let Some(else_block) = &self.false_block {
            else_block.collect_symbols(ctx)
        } else {
            Ok(ctx.take())
        }
    }
}

impl midend::treewalk::Linearize<midend::symtab::ValuePath> for IfExpressionTree {
    type Data = midend::ir::ValueId;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        mut ctx: midend::treewalk::ValueLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        // FUTURE: optimize condition walk to use different jumps
        let condition_loc = self.condition.loc();
        let if_loc = self.loc();
        let condition_value;
        (condition_value, ctx) = self.condition.linearize_in_place(ctx)?;

        let if_condition = midend::ir::lowered::operands::JumpCondition::Conditional(
            midend::ir::lowered::operands::BinaryComparisonOperands::new(
                condition_value,
                *ctx.function_mut().values_mut().id_for_constant(0),
                midend::ir::lowered::operands::BinaryComparisonKind::NE,
            ),
        );

        let parent_scope_def_path = ctx.path().clone();
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
        let if_value_id;
        (if_value_id, ctx) = self.true_block.linearize_in_place(ctx)?;

        // create a separate, mutable value which contains the true result
        let result_value_id = if_value_id;

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
        if let Some(else_block) = self.false_block {
            let else_loc = else_block.loc();
            let else_value_id;
            (else_value_id, ctx) = else_block.linearize_in_place(ctx)?;

            // if the 'else' value exists (have already passed check to assert types are the same)
            // copy the 'else' result to the common result_value at the end of the 'else' block
            let assign_else_result_line =
                midend::ir::IrLine::new_assignment(else_loc.end(), result_value_id, else_value_id);
            ctx.function_mut()
                .append_statement_to_current_block(assign_else_result_line)
                .unwrap();
        };

        ctx.function_mut().finish_branch(if_loc.end()).unwrap();

        ctx.into_result(result_value_id)
    }
}
