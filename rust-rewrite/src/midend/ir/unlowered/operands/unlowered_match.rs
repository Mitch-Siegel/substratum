use crate::midend::{
    ir::unlowered::*,
    linearizer::{CustomReturnWalk, ValueWalk},
    *,
};

struct MatchArmContext<'a> {
    pub ctx: &'a mut linearizer::FunctionWalkContext,
    pub scrutinee: ValueId,
}

struct MatchArmResult<'a> {
    pub arm_label: usize, // the label of the arm to which we should jump if matched
    pub result_value: ValueId,
    pub comparison_value: ValueId,
    pub ctx: &'a mut linearizer::FunctionWalkContext,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct MatchArm {
    pub pattern: frontend::ast::expressions::match_expression::PatternTree,
    pub arm_label: usize,
    pub result_value: ValueId,
}

impl<'a> linearizer::CustomReturnWalk<MatchArmContext<'a>, MatchArmResult<'a>> for MatchArm {
    fn walk(self, arm_context: MatchArmContext<'a>) -> MatchArmResult<'a> {
        let MatchArmContext::<'a> { ctx, scrutinee } = arm_context;

        let (comparison_value, loc) = {
            use frontend::ast::expressions::match_expression::Pattern;
            let (loc, pattern) = (self.pattern.loc, self.pattern.pattern);
            let value = match pattern {
                Pattern::Literal(literal) => literal.walk(ctx),
                Pattern::Identifier(name) => {
                    let (_, def_path) = ctx.lookup_with_path::<symtab::Variable>(&name).unwrap();
                    ctx.values_mut().id_for_variable(def_path)
                }
                Pattern::TupleStruct(name, nested_patterns) => {
                    let scrutinee_value = ctx.values().value_for_id(&scrutinee).unwrap();
                    let scrutinee_type = ctx
                        .symtab()
                        .types
                        .get_definition(&scrutinee_value.ty().unwrap())
                        .unwrap();

                    unimplemented!("tuple struct pattern match not implemented yet");
                }
            };
            (value, loc)
        };

        let comparison_jump = ir::IrLine::new_jump(
            loc,
            self.arm_label,
            lowered::JumpCondition::Conditional(
                ir::lowered::operands::BinaryComparisonOperands::new(
                    scrutinee,
                    comparison_value,
                    lowered::operands::BinaryComparisonKind::EQ,
                ),
            ),
        );
        ctx.append_jump_to_current_block(comparison_jump).unwrap();

        MatchArmResult {
            arm_label: self.arm_label,
            result_value: self.result_value,
            comparison_value,
            ctx,
        }
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct MatchOperands {
    pub scrutinee: ValueId,
    pub arms: Vec<MatchArm>,
}

impl Lowerable for MatchOperands {
    fn lower<'a>(self, mut context: &'a mut linearizer::FunctionWalkContext) {
        for arm in self.arms {
            let result = arm.walk(MatchArmContext::<'a> {
                ctx: context,
                scrutinee: self.scrutinee,
            });

            context = result.ctx;
        }
    }
}
