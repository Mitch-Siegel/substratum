use crate::midend::{ir::unlowered::*, *, linearizer::CustomReturnWalk};


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

impl<'a> linearizer::CustomReturnWalk<MatchArmContext::<'a>, MatchArmResult::<'a>> for MatchArm {
    fn walk(self, arm_context: MatchArmContext<'a>) -> MatchArmResult<'a> {
        let MatchArmContext::<'a> {ctx, scrutinee} = arm_context;

        let comparison_value = ValueId::new(123);

        MatchArmResult {
            arm_label: self.arm_label,
            result_value: self.result_value,
            comparison_value,
            ctx
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
            let result = arm.walk(MatchArmContext::<'a> {ctx: context, scrutinee: self.scrutinee});

            context = result.ctx;

        }
    }
}
