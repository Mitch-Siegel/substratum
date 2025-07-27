use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct AssignmentTree {
    pub loc: SourceLocWithMod,
    pub assignee: Box<ExpressionTree>,
    pub value: Box<ExpressionTree>,
}
impl AssignmentTree {
    pub fn new(loc: SourceLocWithMod, assignee: ExpressionTree, value: ExpressionTree) -> Self {
        Self {
            loc,
            assignee: Box::from(assignee),
            value: Box::from(value),
        }
    }
}
impl Display for AssignmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.assignee, self.value)
    }
}

impl ValueWalk for AssignmentTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> midend::ir::ValueId {
        let assignment_ir = match self.assignee.expression {
            Expression::FieldExpression(field_expression_tree) => {
                let (receiver, field) = field_expression_tree.walk(context);
                let field_name = field.name.clone();
                midend::ir::IrLine::new_field_write(
                    self.value.walk(context).into(),
                    self.loc,
                    receiver.into(),
                    field_name,
                )
            }
            _ => midend::ir::IrLine::new_assignment(
                self.loc,
                self.assignee.walk(context).into(),
                self.value.walk(context).into(),
            ),
        };

        context
            .append_statement_to_current_block(assignment_ir)
            .unwrap();

        context.unit_value_id()
    }
}
