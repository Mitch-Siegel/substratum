use crate::{
    frontend::{ast::*, sourceloc::SourceLoc},
    midend::{linearizer::*, symtab::DefContext, *},
};

use name_derive::NameReflectable;

pub trait Walk {
    fn walk(self, context: &mut impl symtab::DefContext);
}

pub trait ValueWalk {
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId;
}

pub trait BasicReturnWalk<U> {
    fn walk(self, context: &mut symtab::BasicDefContext) -> U;
}

pub trait ReturnWalk<U> {
    fn walk(self, context: &mut impl symtab::DefContext) -> U;
}

pub trait ReturnFunctionWalk<'a, U> {
    fn walk(self, context: &'a mut FunctionWalkContext) -> U;
}

pub trait CustomReturnWalk<C, U> {
    fn walk(self, context: C) -> U;
}

impl CustomReturnWalk<symtab::BasicDefContext, symtab::BasicDefContext> for TranslationUnitTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, mut context: symtab::BasicDefContext) -> symtab::BasicDefContext {
        match self.contents {
            TranslationUnit::FunctionDeclaration(function_declaration) => {
                unimplemented!(
                    "Function declaration without definitions not yet supported: {}",
                    function_declaration.name
                );
                /*
                let function_context = FunctionWalkContext::new(context);
                let declared_function = function_declaration.walk(function_context);

                    function_declaration.walk(&mut WalkContext::new(&context.global_scope));
                context.insert_function_prototype(declared_function);*/
            }
            TranslationUnit::FunctionDefinition(function_definition) => {
                context = function_definition.walk(context);
            }
            TranslationUnit::StructDefinition(struct_definition) => {
                let struct_repr = struct_definition.walk(&mut context);
                context
                    .insert::<symtab::TypeDefinition>(symtab::TypeDefinition::new(
                        types::Type::Named(struct_repr.name.clone()),
                        symtab::TypeRepr::Struct(struct_repr),
                    ))
                    .unwrap();
            }
            TranslationUnit::Implementation(implementation) => {
                context = implementation.walk(context);
            }
        }

        context
    }
}

impl CustomReturnWalk<symtab::BasicDefContext, symtab::BasicDefContext> for ImplementationTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, mut context: symtab::BasicDefContext) -> symtab::BasicDefContext {
        let implemented_for_type = self.type_.walk(&mut context);

        // TODO: modify context's type path

        for item in self.items {
            context = item.walk(context);
        }

        context
    }
}

impl CustomReturnWalk<symtab::BasicDefContext, symtab::BasicDefContext> for FunctionDefinitionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, mut context: symtab::BasicDefContext) -> symtab::BasicDefContext {
        let declared_prototype = self.prototype.walk(&mut context);
        let mut function_context =
            FunctionWalkContext::new(context, declared_prototype, None).unwrap();

        self.body.walk(&mut function_context);

        function_context.into()
    }
}

impl ReturnWalk<types::Type> for TypeTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut impl symtab::DefContext) -> types::Type {
        // TODO: check that the type exists by looking it up
        self.type_
    }
}

impl ValueWalk for VariableDeclarationTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId {
        let variable_type = match self.type_ {
            Some(type_tree) => Some(type_tree.walk(context)),
            None => None,
        };

        let declared_variable: symtab::Variable =
            symtab::Variable::new(self.name.clone(), variable_type);
        let variable_path: symtab::DefPath = context
            .insert::<symtab::Variable>(declared_variable)
            .unwrap();
        *context.value_for_variable(&variable_path)
    }
}

impl ReturnWalk<symtab::Variable> for ArgumentDeclarationTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut impl symtab::DefContext) -> symtab::Variable {
        let variable_type: types::Type = self.type_.walk(context);

        let declared_argument = symtab::Variable::new(self.name.clone(), Some(variable_type));
        declared_argument
    }
}

impl ReturnWalk<symtab::FunctionPrototype> for FunctionDeclarationTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut impl symtab::DefContext) -> symtab::FunctionPrototype {
        symtab::FunctionPrototype::new(
            self.name,
            self.arguments
                .into_iter()
                .map(|x| x.walk(context))
                .collect(),
            match self.return_type {
                Some(type_) => type_.walk(context),
                None => types::Type::Unit,
            },
        )
    }
}

impl ReturnWalk<symtab::StructRepr> for StructDefinitionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut impl symtab::DefContext) -> symtab::StructRepr {
        let fields = self
            .fields
            .into_iter()
            .map(|field| {
                let field_type = field.type_.walk(context);
                (field.name, field_type)
            })
            .collect::<Vec<_>>();
        symtab::StructRepr::new(self.name, fields).unwrap()
    }
}

impl ValueWalk for ArithmeticExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId {
        let (temp_dest, op) = match self {
            ArithmeticExpressionTree::Add(operands) => {
                let lhs: ir::ValueId = operands.e1.walk(context).into();
                let rhs: ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp(None);
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_add(dest, lhs, rhs),
                )
            }
            ArithmeticExpressionTree::Subtract(operands) => {
                let lhs: ir::ValueId = operands.e1.walk(context).into();
                let rhs: ir::ValueId = operands.e2.walk(context).into();
                let dest: ir::ValueId = context.next_temp(None);
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_subtract(dest, lhs, rhs),
                )
            }
            ArithmeticExpressionTree::Multiply(operands) => {
                let lhs: ir::ValueId = operands.e1.walk(context).into();
                let rhs: ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp(None);
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_multiply(dest, lhs, rhs),
                )
            }
            ArithmeticExpressionTree::Divide(operands) => {
                let lhs: ir::ValueId = operands.e1.walk(context).into();
                let rhs: ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp(None);
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_divide(dest, lhs, rhs),
                )
            }
        };

        // TODO: associate location with arithmetic expression trees
        let operation = ir::IrLine::new_binary_op(SourceLoc::none(), op);
        context
            .append_statement_to_current_block(operation)
            .unwrap();
        temp_dest
    }
}

impl ValueWalk for ComparisonExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId {
        let (temp_dest, op) = match self {
            ComparisonExpressionTree::LThan(operands) => {
                let lhs: ir::ValueId = operands.e1.walk(context).into();
                let rhs: ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp(None);
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_lthan(dest, lhs, rhs),
                )
            }
            ComparisonExpressionTree::GThan(operands) => {
                let lhs: ir::ValueId = operands.e1.walk(context).into();
                let rhs: ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp(None);
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_gthan(dest, lhs, rhs),
                )
            }
            ComparisonExpressionTree::LThanE(operands) => {
                let lhs: ir::ValueId = operands.e1.walk(context).into();
                let rhs: ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp(None);
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_lthan_e(dest, lhs, rhs),
                )
            }
            ComparisonExpressionTree::GThanE(operands) => {
                let lhs: ir::ValueId = operands.e1.walk(context).into();
                let rhs: ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp(None);
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_gthan_e(dest, lhs, rhs),
                )
            }
            ComparisonExpressionTree::Equals(operands) => {
                let lhs: ir::ValueId = operands.e1.walk(context).into();
                let rhs: ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp(None);
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_equals(dest, lhs, rhs),
                )
            }
            ComparisonExpressionTree::NotEquals(operands) => {
                let lhs: ir::ValueId = operands.e1.walk(context).into();
                let rhs: ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp(None);
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_not_equals(dest, lhs, rhs),
                )
            }
        };

        // TODO: association location with comparison expression tree
        let operation = ir::IrLine::new_binary_op(SourceLoc::none(), op);
        context
            .append_statement_to_current_block(operation)
            .unwrap();
        temp_dest
    }
}

impl ValueWalk for ExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId {
        match self.expression {
            Expression::Identifier(ident) => {
                let (_, variable_path) = context
                    .lookup_with_path::<symtab::Variable>(&ident)
                    .unwrap();
                *context.value_for_variable(&variable_path)
            }
            Expression::UnsignedDecimalConstant(constant) => {
                *context.value_id_for_constant(constant)
            }
            Expression::Arithmetic(arithmetic_operation) => arithmetic_operation.walk(context),
            Expression::Comparison(comparison_operation) => comparison_operation.walk(context),
            Expression::Assignment(assignment_expression) => assignment_expression.walk(context),
            Expression::If(if_expression) => if_expression.walk(context),
            Expression::While(while_expression) => while_expression.walk(context),
            Expression::FieldExpression(field_expression) => {
                let (receiver, field) = field_expression.walk(context);
                let (field_type, field_name) = (field.type_.clone(), field.name.clone());
                let destination = context.next_temp(Some(field_type));
                let field_read_line = ir::IrLine::new_field_read(
                    self.loc,
                    receiver.into(),
                    field_name,
                    destination.clone(),
                );
                context
                    .append_statement_to_current_block(field_read_line)
                    .unwrap();
                destination
            }
            Expression::MethodCall(method_call) => method_call.walk(context),
        }
    }
}

impl ValueWalk for AssignmentTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId {
        let assignment_ir = match self.assignee.expression {
            Expression::FieldExpression(field_expression_tree) => {
                let (receiver, field) = field_expression_tree.walk(context);
                let field_name = field.name.clone();
                ir::IrLine::new_field_write(
                    self.value.walk(context).into(),
                    self.loc,
                    receiver.into(),
                    field_name,
                )
            }
            _ => ir::IrLine::new_assignment(
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

impl ValueWalk for IfExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId {
        // FUTURE: optimize condition walk to use different jumps
        let condition_loc = self.condition.loc;
        let condition_result: ir::ValueId = self.condition.walk(context).into();
        let if_condition = ir::JumpCondition::NE(ir::operands::DualSourceOperands::new(
            condition_result,
            *context.value_id_for_constant(0),
        ));

        context
            .conditional_branch_from_current(condition_loc, if_condition)
            .unwrap();
        let if_value_id = self.true_block.walk(context);

        // create a separate, mutable value which contains the true result
        let mut result_value = if_value_id.clone();

        // if a false block exists AND the 'if' value exists
        if self.false_block.is_some() {
            // we need to copy the 'if' result to the common result_value at the end of the 'if' block
            let result_value_type = context.type_for_value_id(&result_value).clone();
            let result_value = context.next_temp(Some(result_value_type));
            let assign_if_result_line =
                ir::IrLine::new_assignment(self.loc, result_value, if_value_id);
            context
                .append_statement_to_current_block(assign_if_result_line)
                .unwrap();
        }

        context.finish_true_branch_switch_to_false().unwrap();

        match self.false_block {
            Some(else_block) => {
                let else_value_id = else_block.walk(context);

                // sanity-check that both branches return the same type
                let if_type_id = context.type_id_for_value_id(&if_value_id);
                let else_type_id = context.type_id_for_value_id(&else_value_id);
                if if_type_id != else_type_id {
                    panic!(
                        "If and Else branches return different types ({:?} and {:?}): {}",
                        if_type_id, else_type_id, self.loc
                    );
                }

                // if the 'else' value exists (have already passed check to assert types are the same)
                // copy the 'else' result to the common result_value at the end of the 'else' block
                let assign_else_result_line = ir::IrLine::new_assignment(
                    self.loc,
                    result_value.clone().into(),
                    else_value_id,
                );
                context
                    .append_statement_to_current_block(assign_else_result_line)
                    .unwrap();
            }
            None => {}
        };

        context.finish_branch().unwrap();

        result_value
    }
}

impl ValueWalk for WhileExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId {
        let loop_done_label = context.create_loop(self.loc).unwrap();

        let condition = self.condition.walk(context);
        let loop_condition_jump = ir::IrLine::new_jump(
            self.loc,
            loop_done_label,
            ir::JumpCondition::Eq(ir::DualSourceOperands::new(
                condition.into(),
                *context.value_id_for_constant(0),
            )),
        );

        context
            .append_jump_to_current_block(loop_condition_jump)
            .unwrap();

        context.unconditional_branch_from_current(self.loc).unwrap();
        self.body.walk(context);
        context.finish_branch().unwrap();

        context.finish_loop(self.loc, Vec::new()).unwrap();

        context.unit_value_id()
    }
}

// returns (receiver, field_info)
// receiver is the value id for the receiver of the field access
// field_info is a value id for the field being accessed
impl<'a> ReturnFunctionWalk<'a, (ir::ValueId, &'a symtab::StructField)> for FieldExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &'a mut FunctionWalkContext) -> (ir::ValueId, &'a symtab::StructField) {
        let receiver = self.receiver.walk(context);

        let struct_name = match context.value_for_id(&receiver).unwrap().type_ {
            Some(type_id) => {
                let struct_type = context.type_for_id(&type_id).unwrap();
                match &struct_type.repr {
                    symtab::TypeRepr::Struct(struct_repr) => &struct_repr.name,
                    other_repr => panic!(
                        "Field expression receiver must be of struct type (got {})",
                        other_repr.name()
                    ),
                }
            }
            _ => panic!("unknown type of field receiver",),
        };

        let def_path = context.def_path();
        let receiver_definition = context
            .lookup::<symtab::TypeDefinition>(&types::Type::Named(struct_name.clone()))
            .expect(&format!(
                "Error handling for failed lookups is unimplemented: {}.{}",
                struct_name, self.field
            ));

        let struct_receiver = match &receiver_definition.repr {
            symtab::TypeRepr::Struct(struct_definition) => struct_definition,
            non_struct_repr => panic!("Named type {} is not a struct", non_struct_repr.name()),
        };

        let accessed_field = struct_receiver.lookup_field(&self.field).unwrap();
        (receiver, accessed_field)
    }
}

impl<'a> ReturnFunctionWalk<'a, Vec<ir::ValueId>> for CallParamsTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Vec<ir::ValueId> {
        let mut param_values = Vec::new();

        for param in self.params {
            param_values.push(param.walk(context));
        }

        param_values
    }
}

impl ValueWalk for MethodCallExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId {
        let receiver = self.receiver.walk(context);

        let def_path = context.def_path();
        let receiver_type = context.type_for_value_id(&receiver).clone();
        let called_method = context
            .lookup_implemented_function(&receiver_type, &self.called_method)
            .unwrap();
        let return_type = called_method.prototype.return_type.clone();
        let method_name = String::from(called_method.name());

        let return_value_to = if return_type != types::Type::Unit {
            Some(context.next_temp(Some(return_type)))
        } else {
            None
        };

        // //TODO: error handling and checking
        // assert!(called_method.arguments.len() == params.len());

        let params: Vec<ir::ValueId> = self
            .params
            .walk(context)
            .into_iter()
            .map(|value| value.into())
            .collect();

        let method_call_line = ir::IrLine::new_method_call(
            self.loc,
            receiver.into(),
            &method_name,
            params,
            return_value_to.clone(),
        );

        context
            .append_statement_to_current_block(method_call_line)
            .unwrap();

        match return_value_to {
            Some(value_id) => value_id,
            None => context.unit_value_id(),
        }
    }
}

impl ValueWalk for StatementTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId {
        match self.statement {
            Statement::VariableDeclaration(declaration_tree) => declaration_tree.walk(context),
            Statement::Expression(expression_tree) => expression_tree.walk(context),
        }
    }
}

impl ValueWalk for CompoundExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(mut self, context: &mut FunctionWalkContext) -> ir::ValueId {
        context.unconditional_branch_from_current(self.loc).unwrap();

        let last_statement = self.statements.pop();
        for statement in self.statements {
            statement.walk(context);
        }

        let last_statement_value = match last_statement {
            Some(statement_tree) => match statement_tree.statement {
                Statement::VariableDeclaration(variable_declaration_tree) => {
                    variable_declaration_tree.walk(context);
                    context.unit_value_id()
                }
                Statement::Expression(expression_tree) => expression_tree.walk(context),
            },
            None => context.unit_value_id(),
        };

        context.finish_branch().unwrap();

        last_statement_value
    }
}
