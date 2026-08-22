use std::fmt;

use crate::{
    ast::{Ast, Expression, expressions::BlockExpressionTree},
    sourceloc,
};

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
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
            loc_span = loc_span.merge(&false_block.loc()).unwrap();
        }

        loc_span
    }
}

impl fmt::Display for IfExpressionTree {
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
