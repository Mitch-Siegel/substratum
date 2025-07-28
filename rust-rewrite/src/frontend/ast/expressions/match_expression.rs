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

impl<'a> ReturnFunctionWalk<'a, midend::ir::ValueId> for PatternTree {
    fn walk(self, context: &'a mut FunctionWalkContext) -> midend::ir::ValueId {
        match self.pattern {
            Pattern::LiteralPattern(literal_expression) => literal_expression.walk(context),
            Pattern::IdentifierPattern(identifier) => *context.value_for_variable(
                &context
                    .def_path()
                    .with_component(midend::symtab::DefPathComponent::Variable(identifier))
                    .unwrap(),
            ),
            Pattern::TupleStructPattern(_struct_name, _fields) => {
                unimplemented!()
            }
        }
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
impl<'a> ReturnFunctionWalk<'a, (midend::ir::ValueId, BlockExpressionTree)> for MatchArmTree {
    fn walk(
        self,
        context: &'a mut FunctionWalkContext,
    ) -> (midend::ir::ValueId, BlockExpressionTree) {
        let pattern_value = self.pattern.walk(context);
        (pattern_value, self.expression)
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

impl ValueWalk for MatchExpressionTree {
    fn walk(self, context: &mut midend::linearizer::FunctionWalkContext) -> midend::ir::ValueId {
        let _scrutinee_value = self.scrutinee_expression.walk(context);
        let result_value = context.next_temp(None);

        for arm in self.arms {
            let _pattern_loc = arm.loc.clone();
            let (_matched_value, _expression) = arm.walk(context);
        }

        result_value
    }
}
