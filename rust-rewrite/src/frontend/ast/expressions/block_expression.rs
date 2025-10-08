use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct BlockExpressionTree {
    pub loc: SourceLoc,
    pub statements: Vec<StatementTree>,
}
impl BlockExpressionTree {
    pub fn new(loc: SourceLoc, statements: Vec<StatementTree>) -> Self {
        Self { loc, statements }
    }
}
impl Display for BlockExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut statement_string = String::from("");
        for statement in &self.statements {
            statement_string.push_str(format!("{}\n", statement).as_str());
        }
        write!(f, "Block Expression: {}", statement_string)
    }
}

impl treewalk::CollectSymbols for BlockExpressionTree {
    fn collect_symbols(&self, ctx: &mut treewalk::CollectCtx) {
        unimplemented!();
    }
}

impl treewalk::Linearize<midend::ir::ValueId> for BlockExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(mut self, ctx: &mut treewalk::LinearizeCtx) -> midend::ir::ValueId {
        let parent_def_path = ctx.def_path().clone();
        let true_scope_def_path = ctx.reserve_subscope();
        ctx.function_mut()
            .unconditional_branch_from_current(
                self.loc.clone(),
                parent_def_path,
                true_scope_def_path,
            )
            .unwrap();

        let last_statement = self.statements.pop();
        for statement in self.statements {
            statement.linearize(ctx);
        }

        let last_statement_value = match last_statement {
            Some(statement_tree) => statement_tree.linearize(ctx),
            None => midend::ir::ValueInterner::unit_value_id(),
        };

        ctx.function_mut().finish_branch(self.loc).unwrap();

        last_statement_value
    }
}
