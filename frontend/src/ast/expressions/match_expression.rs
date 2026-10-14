use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression, IdentifierTree, expressions},
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TupleStructTree {
    pub name: IdentifierTree,
    pub subpatterns: Vec<PatternTree>,
    pub(crate) close_paren: loc::Span,
}

impl Ast for TupleStructTree {
    fn span(&self) -> loc::Span {
        self.name.span().merge(&self.close_paren).unwrap()
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
    fn span(&self) -> loc::Span {
        match self {
            Self::Literal(e) => e.span(),
            Self::Identifier(i) => i.span(),
            Self::TupleStruct(t) => t.span(),
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
    fn span(&self) -> loc::Span {
        self.pattern.span().merge(&self.expression.span()).unwrap()
    }
}

impl fmt::Display for MatchArmTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} => {}", self.pattern, self.expression)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MatchExpressionTree {
    pub(crate) match_keyword: loc::Span,
    pub scrutinee_expression: Expression,
    pub arms: Vec<MatchArmTree>,
}

impl Ast for MatchExpressionTree {
    fn span(&self) -> loc::Span {
        let mut loc = self
            .match_keyword
            .clone()
            .merge(&self.scrutinee_expression.span())
            .unwrap();

        for arm in &self.arms {
            loc = loc.merge(&arm.span()).unwrap();
        }

        loc
    }
}

impl fmt::Display for MatchExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "match {} {{{:?}}}", self.scrutinee_expression, self.arms)
    }
}
