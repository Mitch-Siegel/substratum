use crate::frontend::ast::{expressions::*, *};

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct IfExpressionTree {
    pub loc: SourceLocWithMod,
    pub condition: ExpressionTree,
    pub true_block: BlockExpressionTree,
    pub false_block: Option<BlockExpressionTree>,
}
impl IfExpressionTree {
    pub fn new(
        loc: SourceLocWithMod,
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
