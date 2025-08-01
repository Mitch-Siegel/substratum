use crate::{frontend::ast::expressions::*, trace};

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum Pattern {
    LiteralPattern(ExpressionTree),
    IdentifierPattern(String),
    // TODO: PathInExpression
    TupleStructPattern(String, Vec<PatternTree>),
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

struct MatchArmWalkContext<'a> {
    pub ctx: &'a mut FunctionWalkContext,
    pub scrutinee: midend::ir::ValueId,
}

fn destructure_tuple(
    context: &FunctionWalkContext,
    value: midend::ir::ValueId,
    element_patterns: Vec<PatternTree>,
) {
    let tuple_type = match context.type_for_value_id(&value) {
        Some(type_) => type_,
        None => panic!("Undefined tuple tupe"),
    };

    match tuple_type {
        midend::types::Type::Tuple(elements) => {
            if elements.len() != element_patterns.len() {
                panic!(
                    "Expected {} elements in tuple pattern destructuring, found {}",
                    elements.len(),
                    element_patterns.len()
                );
            }

            for (element, pattern) in elements.iter().zip(element_patterns.iter()) {
                trace::warning!("element: {:?}, pattern {:?}", element, pattern);
            }
        }
        other => panic!("Unexpected type {} - expected tuple to destructure", other),
    }
}

fn destructure_enum_variant(
    context: &mut FunctionWalkContext,
    enum_value: midend::ir::ValueId,
    enum_def: &midend::symtab::EnumRepr,
    variant_name: String,
    elements: Vec<PatternTree>,
) {
    let variant = match enum_def.get_variant(&variant_name) {
        Some(exists) => exists,
        None => panic!("Enum {} has no variant {}", enum_def.name, variant_name),
    };

    let variant_ptr_value = context.next_temp(Some(variant.type_()));

    let variant_access_line = midend::ir::IrLine::new_get_field_pointer(
        SourceLocWithMod::none(),
        enum_value,
        variant_name,
        variant_ptr_value,
    );
    context
        .append_statement_to_current_block(variant_access_line)
        .unwrap();

    destructure_tuple(context, variant_ptr_value, elements);
}

impl<'a> CustomReturnWalk<MatchArmWalkContext<'a>, midend::ir::ValueId> for PatternTree {
    fn walk(self, context: MatchArmWalkContext) -> midend::ir::ValueId {
        match self.pattern {
            Pattern::LiteralPattern(literal_expression) => literal_expression.walk(context.ctx),
            Pattern::IdentifierPattern(identifier) => {
                let variable = midend::symtab::Variable::new(
                    identifier,
                    context.ctx.type_for_value_id(&context.scrutinee),
                );

                let identifier_binding = context
                    .ctx
                    .insert::<midend::symtab::Variable>(variable)
                    .unwrap();

                let binding_value = context
                    .ctx
                    .id_for_variable_or_insert(identifier_binding)
                    .clone();

                let binding_assignment_line =
                    midend::ir::IrLine::new_assignment(self.loc, binding_value, context.scrutinee);

                context
                    .ctx
                    .append_statement_to_current_block(binding_assignment_line)
                    .unwrap();

                binding_value
            }

            Pattern::TupleStructPattern(_struct_name, _fields) => {
                let destructured_definition =
                    match context.ctx.type_definition_for_value_id(&context.scrutinee) {
                        Some(definition) => definition,
                        None => panic!("Tuple destructuring on unknown type ({})", self.loc),
                    };

                match &destructured_definition.repr {
                    midend::symtab::TypeRepr::Enum(repr) => destructure_enum_variant(
                        context.ctx,
                        context.scrutinee,
                        &repr.clone(),
                        _struct_name,
                        _fields,
                    ),
                    other => panic!("can't destructure non-enum repr {}", other.name()),
                }

                midend::ir::ValueId::new(123)
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
impl<'a> CustomReturnWalk<MatchArmWalkContext<'a>, (midend::ir::ValueId, BlockExpressionTree)>
    for MatchArmTree
{
    fn walk(self, context: MatchArmWalkContext<'a>) -> (midend::ir::ValueId, BlockExpressionTree) {
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
        context.create_switch(self.loc).unwrap();

        let scrutinee_value = self.scrutinee_expression.walk(context);
        let result_value = context.next_temp(None);

        let mut arm_values = Vec::new();

        for arm in self.arms {
            let case_label = context.create_switch_case().unwrap();
            let _pattern_loc = arm.loc.clone();
            let arm_context = MatchArmWalkContext {
                ctx: context,
                scrutinee: scrutinee_value.clone(),
            };
            let (_matched_value, expression) = arm.walk(arm_context);
            arm_values.push(expression.walk(context));
            context.finish_switch_case().unwrap();

            let case_jump = midend::ir::IrLine::new_jump(
                _pattern_loc,
                case_label,
                midend::ir::JumpCondition::Eq(midend::ir::DualSourceOperands::new(
                    _matched_value,
                    scrutinee_value,
                )),
            );
            context.append_jump_to_current_block(case_jump).unwrap();
        }

        result_value
    }
}
