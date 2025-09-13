use crate::midend::{ir::unlowered::*, *};

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct MatchArm {
    pub pattern: frontend::ast::expressions::match_expression::PatternTree,
    pub arm_label: usize,
    pub result_value: ValueId,
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct MatchOperands {
    pub scrutinee: ValueId,
    pub arms: Vec<MatchArm>,
}

impl Lowerable for MatchOperands {
    fn lower(self, context: &mut linearizer::FunctionWalkContext) {
        context
            .append_statement_to_current_block(IrLine::new_load(
                SourceLoc::new(&std::path::Path::new("asdf"), 9999, 9999),
                ValueId::new(1234),
                ValueId::new(1235),
            ))
            .unwrap();
    }
}
