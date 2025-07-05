use crate::{
    frontend::{ast::*, sourceloc::SourceLoc},
    midend::{*, linearizer::*},
};

use name_derive::NameReflectable;

pub trait Walk {
    fn walk(self, context: &mut impl symtab::DefContext);
}

pub trait ValueWalk {
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId;
}

pub trait ReturnWalk<U> {
    fn walk(self, context: &mut impl symtab::DefContext) -> U;
}

impl Walk for TranslationUnitTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut symtab::BasicDefContext) {
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
                let defined_function = function_definition.walk(context).unwrap();
                context
                    .insert::<symtab::Function>(defined_function)
                    .unwrap();
            }
            TranslationUnit::StructDefinition(struct_definition) => {
                let struct_repr = struct_definition.walk(context);
                context
                    .insert::<symtab::TypeDefinition>(symtab::TypeDefinition::new(
                        Type::Named(struct_repr.name.clone()),
                        symtab::TypeRepr::Struct(struct_repr),
                    ))
                    .unwrap();
            }
            TranslationUnit::Implementation(implementation) => {
                implementation.walk(context);
            }
        }
    }
}

impl Walk for ImplementationTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut symtab::BasicDefContext) {
        let implemented_for_type = self.type_.walk().type_;

        let methods: Vec<symtab::Function> = self
            .items
            .into_iter()
            .map(|item| item.walk(context).unwrap())
            .collect();

        let def_path = context.def_path();
        let (implemented_for, implemented_for_path) = context
            .lookup_with_path::<symtab::TypeDefinition>(&implemented_for_type)
            .unwrap();

        for method in methods {
            context
                .symtab_mut()
                .insert::<symtab::Function>(implemented_for_path, method)
                .unwrap();
        }
    }
}

impl<'a>
    ContextReturnWalk<symtab::BasicDefContext<'a>, Result<symtab::Function, symtab::SymbolError>>
    for FunctionDefinitionTree
{
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut symtab::BasicDefContext) {
        let declared_prototype = self.prototype.walk();
        let mut function_context =
            FunctionWalkContext::new(context, declared_prototype, None).unwrap();

        self.body.walk(&mut function_context);
    }
}

impl ReturnWalk<Value> for TypeTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self) -> Value {
        // check that the type exists by looking it up
        Value::from_type(self.type_)
    }
}

impl ReturnWalk<symtab::Variable> for VariableDeclarationTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self) -> symtab::Variable {
        let variable_type = match self.type_ {
            Some(type_tree) => Some(type_tree.walk().type_),
            None => None,
        };

        symtab::Variable::new(self.name.clone(), variable_type)
    }
}

impl ReturnWalk<symtab::Variable> for ArgumentDeclarationTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self) -> symtab::Variable {
        symtab::Variable::new(self.name.clone(), Some(self.type_.walk().type_))
    }
}

impl<'a> ReturnWalk<symtab::FunctionPrototype> for FunctionDeclarationTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self) -> symtab::FunctionPrototype {
        symtab::FunctionPrototype::new(
            self.name,
            self.arguments.into_iter().map(|x| x.walk()).collect(),
            match self.return_type {
                Some(type_) => type_.walk().type_,
                None => Type::Unit,
            },
        )
    }
}

impl ReturnWalk<symtab::StructRepr> for StructDefinitionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self) -> symtab::StructRepr {
        let fields = self
            .fields
            .into_iter()
            .map(|field| {
                let field_type = field.type_.walk().type_;
                (field.name, field_type)
            })
            .collect::<Vec<_>>();
        symtab::StructRepr::new(self.name, fields).unwrap()
    }
}

impl Walk for ArithmeticExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Value {
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
        let operation = IrLine::new_binary_op(SourceLoc::none(), op);
        context
            .append_statement_to_current_block(operation)
            .unwrap();
        Value::from_operand(temp_dest, context)
    }
}

impl Walk for ComparisonExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Value {
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
        let operation = IrLine::new_binary_op(SourceLoc::none(), op);
        context
            .append_statement_to_current_block(operation)
            .unwrap();
        Value::from_operand(temp_dest, context)
    }
}

impl ValueWalk for ExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId {
        match self.expression {
            Expression::Identifier(ident) => {
                let (_, variable_path) = context.lookup_with_path::<symtab::Variable>(ident).unwrap();
                context.value_for_variable(variable_path)
            }
            Expression::UnsignedDecimalConstant(constant) => context.value_id_for_constant(constant)
            Expression::Arithmetic(arithmetic_operation) => arithmetic_operation.walk(context),
            Expression::Comparison(comparison_operation) => comparison_operation.walk(context),
            Expression::Assignment(assignment_expression) => assignment_expression.walk(context),
            Expression::If(if_expression) => if_expression.walk(context),
            Expression::While(while_expression) => while_expression.walk(context),
            Expression::FieldExpression(field_expression) => {
                let (receiver, field) = field_expression.walk(context);
                let destination = context.next_temp(field.type_);
                let field_read_line = ir::IrLine::new_field_read(
                    self.loc,
                    receiver.into(),
                    field.operand.unwrap().get_name().unwrap().base_name.clone(),
                    destination.clone(),
                );
                context
                    .append_statement_to_current_block(field_read_line)
                    .unwrap();
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
                ir::IrLine::new_field_write(
                    self.value.walk(context).into(),
                    self.loc,
                    receiver.into(),
                    field.operand.unwrap().get_name().unwrap().base_name.clone(),
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
            context.value_id_for_constant(0)
        ));

        context
            .conditional_branch_from_current(condition_loc, if_condition)
            .unwrap();
        let if_value = self.true_block.walk(context);

        // create a separate, mutable value which contains the true result
        let mut result_value = if_value.clone();

        // if a false block exists AND the 'if' value exists
        if self.false_block.is_some() && if_value.operand.is_some() {
            // we need to copy the 'if' result to the common result_value at the end of the 'if' block
            let result_value= context.next_temp(if_value.type_.clone());
            let assign_if_result_line =
                ir::IrLine::new_assignment(self.loc, result_value, if_value);
            context
                .append_statement_to_current_block(assign_if_result_line)
                .unwrap();
        }

        context.finish_true_branch_switch_to_false().unwrap();

        match self.false_block {
            Some(else_block) => {
                let else_value = else_block.walk(context);

                // sanity-check that both branches return the same type
                if if_value.type_ != else_value.type_ {
                    panic!(
                        "If and Else branches return different types ({} and {}): {}",
                        if_value.type_, else_value.type_, self.loc
                    );
                }

                // if the 'else' value exists (have already passed check to assert types are the same)
                if else_value.operand.is_some() {
                    // copy the 'else' result to the common result_value at the end of the 'else' block
                    let assign_else_result_line = ir::IrLine::new_assignment(
                        self.loc,
                        result_value.clone().into(),
                        else_value.into(),
                    );
                    context
                        .append_statement_to_current_block(assign_else_result_line)
                        .unwrap();
                }
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
                ir::ValueId::new_as_unsigned_decimal_constant(0),
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
impl ReturnWalk<(ir::ValueId, String)> for FieldExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> (ir::ValueId, ir::ValueId) {
        let receiver = self.receiver.walk(context);

        let struct_name = match &receiver.type_ {
            types::Type::Named(type_name) => type_name,
            _ => panic!(
                "Field expression receiver must be of struct type (got {})",
                receiver.type_
            ),
        };

        let def_path = context.def_path();
        let receiver_definition = context
            .lookup::<symtab::TypeDefinition>(&types::Type::Named(struct_name.into()))
            .expect(&format!(
                "Error handling for failed lookups is unimplemented: {}.{}",
                receiver.type_, self.field
            ));

        let struct_receiver = match receiver_definition.repr {
            symtab::TypeRepr::Struct(struct_definition) => struct_definition,
            non_struct_repr => panic!("Named type {} is not a struct", non_struct_repr.name()), 
        }

        let accessed_field = struct_receiver.lookup_field(&self.field).unwrap();
        (
            receiver,
            accessed_field.name.clone()
        )
    }
}

impl<'a> ReturnWalk<Vec<ir::ValueId>> for CallParamsTree {
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
        let type_definition = context
            .symtab()
            .lookup_type(&def_path, &receiver.type_)
            .unwrap();
        let called_method: symtab::FunctionPrototype = unimplemented!();
        /*type_definition
        .lookup_method(def_path, &self.called_method)
        .unwrap()
        .prototype
        .clone();*/

        let return_value_to = if called_method.return_type != types::Type::Unit {
            Some(context.next_temp(called_method.return_type))
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
            &called_method.name,
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
            Statement::VariableDeclaration(declaration_tree) => {
                let declared_variable = declaration_tree.walk(context);
                let def_path = context.def_path();
                context
                    .symtab_mut()
                    .insert::<symtab::Variable>(def_path, declared_variable)
                    .unwrap();
                context.unit_value_id()
                
            }
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
