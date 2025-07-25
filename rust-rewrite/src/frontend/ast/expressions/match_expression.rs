use crate::frontend::ast::expressions::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum Pattern {
    LiteralPattern(ExpressionTree),
    IdentifierPattern(String),
    // TODO: PathInExpression
    TupleStructPattern(String, Vec<Box<PatternTree>>),
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct PatternTree {
    pub loc: SourceLocWithMod,
    pub pattern: Pattern,
}
impl PatternTree {
    pub fn new(loc: SourceLocWithMod, pattern: Pattern) -> Self {
        Self { loc, pattern }
    }
}

impl Display for PatternTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:?}", self.pattern)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct MatchArmTree {
    pub loc: SourceLocWithMod,
    pub pattern: PatternTree,
    pub expression: BlockExpressionTree,
}
impl MatchArmTree {
    pub fn new(
        loc: SourceLocWithMod,
        pattern: PatternTree,
        expression: BlockExpressionTree,
    ) -> Self {
        Self {
            loc,
            pattern,
            expression,
        }
    }
}
impl Display for MatchArmTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} => {}", self.pattern, self.expression)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct MatchExpressionTree {
    pub loc: SourceLocWithMod,
    pub scrutinee_expression: ExpressionTree,
    pub arms: Vec<MatchArmTree>,
}
impl MatchExpressionTree {
    pub fn new(
        loc: SourceLocWithMod,
        scrutinee_expression: ExpressionTree,
        arms: Vec<MatchArmTree>,
    ) -> Self {
        Self {
            loc,
            scrutinee_expression,
            arms,
        }
    }
}
impl Display for MatchExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "match {} {{{:?}}}", self.scrutinee_expression, self.arms)
    }
}
