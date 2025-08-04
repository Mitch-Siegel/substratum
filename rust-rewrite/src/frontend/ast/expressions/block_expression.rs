use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
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

impl ValueWalk for BlockExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(mut self, context: &mut FunctionWalkContext) -> midend::ir::ValueId {
        context.unconditional_branch_from_current(self.loc).unwrap();

        let last_statement = self.statements.pop();
        for statement in self.statements {
            statement.walk(context);
        }

        let last_statement_value = match last_statement {
            Some(statement_tree) => statement_tree.walk(context),
            None => context.unit_value_id(),
        };

        context.finish_branch().unwrap();

        last_statement_value
    }
}
