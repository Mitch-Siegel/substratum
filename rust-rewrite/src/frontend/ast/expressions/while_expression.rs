use crate::frontend::ast::{expressions::*, *};

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct WhileExpressionTree {
    pub loc: SourceLocWithMod,
    pub condition: ExpressionTree,
    pub body: BlockExpressionTree,
}
impl WhileExpressionTree {
    pub fn new(
        loc: SourceLocWithMod,
        condition: ExpressionTree,
        body: BlockExpressionTree,
    ) -> Self {
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
