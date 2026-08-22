use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression, IdentifierTree, expressions},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TupleStructTree {
    pub name: IdentifierTree,
    pub subpatterns: Vec<PatternTree>,
    pub(crate) close_paren_loc: sourceloc::SourceSpan,
}

impl Ast for TupleStructTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.name.loc().merge(&self.close_paren_loc).unwrap()
    }
}

impl fmt::Display for TupleStructTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}(", self.name)?;
        let mut first = true;
        for subpattern in &self.subpatterns {
            if first {
                write!(f, "{subpattern}")?;
                first = false;
            } else {
                write!(f, ", {subpattern}")?;
            }
        }

        write!(f, ")")
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum PatternTree {
    Literal(Expression),
    Identifier(IdentifierTree),
    // TODO: PathInExpression
    TupleStruct(TupleStructTree),
}

impl Ast for PatternTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::Literal(e) => e.loc(),
            Self::Identifier(i) => i.loc(),
            Self::TupleStruct(t) => t.loc(),
        }
    }
}

impl fmt::Display for PatternTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Literal(e) => write!(f, "{e}"),
            Self::Identifier(i) => write!(f, "{i}"),
            Self::TupleStruct(t) => write!(f, "{t}"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MatchArmTree {
    pub pattern: PatternTree,
    pub expression: expressions::BlockExpressionTree,
}

impl Ast for MatchArmTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.pattern.loc().merge(&self.expression.loc()).unwrap()
    }
}

impl fmt::Display for MatchArmTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} => {}", self.pattern, self.expression)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MatchExpressionTree {
    pub(crate) match_keyword_loc: sourceloc::SourceSpan,
    pub scrutinee_expression: Expression,
    pub arms: Vec<MatchArmTree>,
}

impl Ast for MatchExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut loc = self
            .match_keyword_loc
            .clone()
            .merge(&self.scrutinee_expression.loc())
            .unwrap();

        for arm in &self.arms {
            loc = loc.merge(&arm.loc()).unwrap();
        }

        loc
    }
}

impl fmt::Display for MatchExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "match {} {{{:?}}}", self.scrutinee_expression, self.arms)
    }
}
