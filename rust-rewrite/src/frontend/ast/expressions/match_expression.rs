use crate::{frontend::ast::expressions::*, trace};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum Pattern {
    LiteralPattern(ExpressionTree),
    IdentifierPattern(String),
    // TODO: PathInExpression
    TupleStructPattern(String, Vec<PatternTree>),
}

impl<'a> ReturnFunctionWalk<'a, ()> for Pattern {
    fn walk(self, context: &'a mut FunctionWalkContext) -> () {
        match self {
            Self::LiteralPattern(_) => (),
            Self::IdentifierPattern(name) => {
                context
                    .insert::<midend::symtab::Variable>(midend::symtab::Variable::new(
                        name.clone(),
                        None,
                    ))
                    .unwrap();
            }
            Self::TupleStructPattern(_struct_name, field_patterns) => {
                for field in field_patterns.clone() {
                    field.walk(context);
                }
            }
        };
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct PatternTree {
    pub loc: SourceLoc,
    pub pattern: Pattern,
}
impl PatternTree {
    pub fn new(loc: SourceLoc, pattern: Pattern) -> Self {
        Self { loc, pattern }
    }
}

impl Display for PatternTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:?}", self.pattern)
    }
}

impl CustomReturnWalk<&mut FunctionWalkContext, PatternTree> for PatternTree {
    fn walk(self, context: &mut FunctionWalkContext) -> PatternTree {
        self.pattern.clone().walk(context);
        self
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct MatchArmTree {
    pub loc: SourceLoc,
    pub pattern: PatternTree,
    pub expression: BlockExpressionTree,
}
impl MatchArmTree {
    pub fn new(loc: SourceLoc, pattern: PatternTree, expression: BlockExpressionTree) -> Self {
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
impl<'a> ReturnFunctionWalk<'a, (PatternTree, midend::ir::ValueId)> for MatchArmTree {
    fn walk(self, context: &'a mut FunctionWalkContext) -> (PatternTree, midend::ir::ValueId) {
        let pattern = self.pattern.walk(context);
        let arm_value = self.expression.walk(context);
        (pattern, arm_value)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct MatchExpressionTree {
    pub loc: SourceLoc,
    pub scrutinee_expression: ExpressionTree,
    pub arms: Vec<MatchArmTree>,
}

impl MatchExpressionTree {
    pub fn new(
        loc: SourceLoc,
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
        let match_loc = self.loc;
        context.create_switch(match_loc.clone()).unwrap();

        let scrutinee_value = self.scrutinee_expression.walk(context);

        // TODO: consolidate each arm's result into result_value
        let result_value = context.next_temp();

        let mut arm_values = Vec::new();

        for arm in self.arms {
            let arm_label = context.create_switch_case().unwrap();
            let _pattern_loc = arm.loc.clone();
            let (pattern, result_value) = arm.walk(context);
            context.finish_switch_case().unwrap();

            arm_values.push(midend::ir::unlowered::operands::MatchArm {
                pattern,
                arm_label,
                result_value,
            });
        }

        context
            .append_statement_to_current_block(midend::ir::IrLine::new_match(match_loc, arm_values))
            .unwrap();

        result_value
    }
}
