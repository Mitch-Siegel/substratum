use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct BlockExpressionTree {
    pub loc: SourceLocWithMod,
    pub statements: Vec<StatementTree>,
}
impl BlockExpressionTree {
    pub fn new(loc: SourceLocWithMod, statements: Vec<StatementTree>) -> Self {
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
