use crate::{frontend::ast::expressions::*, trace};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum Pattern {
    Literal(ExpressionTree),
    Identifier(String),
    // TODO: PathInExpression
    TupleStruct(String, Vec<PatternTree>),
}

impl<'a> ReturnFunctionWalk<'a, ()> for Pattern {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &'a mut FunctionWalkContext) -> () {
        match self {
            Self::Literal(_) => (),
            Self::Identifier(name) => {
                let variable_def_path = context
                    .insert::<midend::symtab::Variable>(midend::symtab::Variable::new(
                        name.clone(),
                        None,
                    ))
                    .unwrap();
                // TODO: examine if there's a better way to just declare variables and give them a
                // ValueID in one go?
                context.values_mut().id_for_variable(variable_def_path);
            }
            Self::TupleStruct(_struct_name, field_patterns) => {
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
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
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
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
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
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut midend::linearizer::FunctionWalkContext) -> midend::ir::ValueId {
        let match_loc = self.loc;
        context.create_switch(match_loc.clone()).unwrap();

        let scrutinee_value = self.scrutinee_expression.walk(context);

        // TODO: consolidate each arm's result into result_value
        let result_value = context.values_mut().next_temp();

        let mut arm_values = Vec::new();

        for arm in self.arms {
            trace::warning!("start arm");
            let arm_label = context.create_switch_case().unwrap();
            let _pattern_loc = arm.loc.clone();
            let (pattern, result_value) = arm.walk(context);
            context.finish_switch_case(match_loc.clone()).unwrap();

            arm_values.push(midend::ir::unlowered::operands::MatchArm {
                pattern,
                arm_label,
                result_value,
            });
            trace::warning!("finish arm");
        }

        context
            .append_statement_to_current_block(midend::ir::IrLine::new_match(
                context.def_path(),
                match_loc.clone(),
                scrutinee_value,
                arm_values,
            ))
            .unwrap();

        // FIXME: (?) Convergence currently exists from the switch block itself to the after-switch
        // block, resulting in an unreachable jump instruction after the unlowered match IR.
        context.finish_switch(match_loc).unwrap();
        result_value
    }
}
