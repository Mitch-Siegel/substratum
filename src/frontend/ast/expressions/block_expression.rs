use crate::{
    frontend::ast::{
        midend, sourceloc, symtab, treewalk, Ast, Display, NameReflectable, ReflectName,
        StatementTree,
    },
    midend::{symtab::ValuePath, treewalk::PathedCtxTrait},
};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct BlockExpressionTree {
    pub open_brace_loc: sourceloc::SourceSpan,
    pub statements: Vec<StatementTree>,
    pub close_brace_loc: sourceloc::SourceSpan,
}

impl Ast for BlockExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_brace_loc
            .clone()
            .merge(&self.close_brace_loc.clone())
            .unwrap()
    }
}

impl Display for BlockExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut statement_string = String::new();
        for statement in &self.statements {
            statement_string.push_str(format!("{statement}\n").as_str());
        }
        write!(f, "Block Expression: {statement_string}")
    }
}

impl midend::treewalk::Collect<ValuePath> for BlockExpressionTree {
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        for stmt in &self.statements {
            ctx = stmt.collect_symbols(ctx)?;
        }

        ctx.into_result()
    }
}

impl
    treewalk::Linearize<
        treewalk::UnpathedFunctionLinearizeCtx,
        symtab::ScopePath,
        treewalk::FunctionLinearizeCtx,
    > for BlockExpressionTree
{
    type Data = midend::ir::ValueId;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        mut self,
        mut ctx: treewalk::FunctionLinearizeCtx,
    ) -> treewalk::LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        let parent_def_path = ctx.path().clone();
        let true_scope_def_path = ctx.reserve_subscope();
        ctx.function_mut().unconditional_branch_from_current(
            self.open_brace_loc.start(),
            parent_def_path,
            true_scope_def_path,
        );

        let last_statement = self.statements.pop();
        for statement in self.statements {
            (_, ctx) = statement.linearize(ctx)?;
        }

        let last_statement_value = match last_statement {
            Some(statement_tree) => {
                let maybe_value;
                (maybe_value, ctx) = statement_tree.linearize(ctx)?;
                maybe_value.unwrap_or(midend::ir::ValueInterner::unit_value_id())
            }
            None => midend::ir::ValueInterner::unit_value_id(),
        };

        ctx.function_mut()
            .finish_branch(self.close_brace_loc.end())
            .unwrap();

        ctx.into_result(last_statement_value)
    }
}
