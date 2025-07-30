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
                let field_loc = field_expression_tree.loc.clone();
                let (receiver, field) = field_expression_tree.walk(context);
                let field_name = field.name.clone();
                let field_type = field.type_.clone();

                let field_pointer_temp = context.next_temp(Some(field_type));

                let field_pointer_line = midend::ir::IrLine::new_get_field_pointer(
                    field_loc,
                    receiver,
                    field_name,
                    field_pointer_temp,
                );
                context
                    .append_statement_to_current_block(field_pointer_line)
                    .unwrap();

                midend::ir::IrLine::new_store(
                    self.loc,
                    self.value.walk(context).into(),
                    field_pointer_temp,
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
