use crate::{
    frontend::{ast::*, sourceloc::SourceLoc},
    midend::{
        ir::{self, IrLine},
        symtab::{self, ScopedLookups, SymbolTable},
        types::Type,
    },
};

use super::walkcontext::WalkContext;

#[derive(Clone, Debug)]
pub struct Value {
    pub type_: Type,
    pub operand: Option<ir::Operand>,
}

impl Value {
    pub fn unit() -> Self {
        Self {
            type_: Type::Unit,
            operand: None,
        }
    }

    pub fn from_type(type_: Type) -> Self {
        Self {
            type_,
            operand: None,
        }
    }

    pub fn from_type_and_name(type_: Type, operand: ir::Operand) -> Self {
        Self {
            type_,
            operand: Some(operand),
        }
    }

    pub fn from_operand(operand: ir::Operand, context: &WalkContext) -> Self {
        Self {
            type_: operand.type_(context).clone(),
            operand: Some(operand),
        }
    }
}

impl Into<ir::Operand> for Value {
    fn into(self) -> ir::Operand {
        self.operand.unwrap()
    }
}

impl Into<Type> for Value {
    fn into(self) -> Type {
        self.type_
    }
}

pub trait TableWalk {
    fn walk(self, symbol_table: &mut SymbolTable);
}

pub trait Walk {
    fn walk(self, context: &mut WalkContext) -> Value;
}

pub trait ReturnWalk<T, U> {
    fn walk(self, context: T) -> U;
}

impl TableWalk for TranslationUnitTree {
    fn walk(self, symbol_table: &mut SymbolTable) {
        match self.contents {
            TranslationUnit::FunctionDeclaration(function_declaration) => {
                let declared_function =
                    function_declaration.walk(&mut WalkContext::new(&symbol_table.global_scope));
                symbol_table.insert_function_prototype(declared_function);
            }
            TranslationUnit::FunctionDefinition(function_definition) => {
                symbol_table.insert_function(function_definition.walk(&symbol_table.global_scope));
            }
            TranslationUnit::StructDefinition(struct_definition) => {
                let mut defined_struct = symtab::StructRepr::new(struct_definition.name);
                for field in struct_definition.fields {
                    // TODO: global scoping
                    let field_type = field
                        .type_
                        .walk(&mut WalkContext::new(&symbol_table.global_scope));
                    defined_struct.add_field(field.name, field_type.type_);
                }

                symbol_table
                    .global_scope
                    .insert_struct_definition(defined_struct);
            }
            TranslationUnit::Implementation(implementation) => {
                let mut impl_context = WalkContext::new(&symbol_table.global_scope);
                let implemented_for_type = implementation.type_.walk(&mut impl_context).type_;

                let methods: Vec<symtab::Function> = implementation
                    .items
                    .into_iter()
                    .map(|item| item.walk(&symbol_table.global_scope))
                    .collect();

                let implemented_for_mut = symbol_table
                    .global_scope
                    .lookup_type_mut(&implemented_for_type)
                    .unwrap();

                for method in methods {
                    implemented_for_mut.add_method(method);
                }
            }
        }
    }
}

impl<'a> ReturnWalk<&'a symtab::Scope, symtab::Function> for FunctionDefinitionTree {
    fn walk(self, global_scope: &'a symtab::Scope) -> symtab::Function {
        let mut context = WalkContext::new(global_scope);
        let mut declared_prototype = self.prototype.walk(&mut context);
        context.push_scope(declared_prototype.create_argument_scope());

        self.body.walk(&mut context);
        let argument_scope = context.pop_last_scope();

        symtab::Function::new(
            declared_prototype,
            argument_scope,
            context.take_control_flow(),
        )
    }
}

impl Walk for TypeTree {
    fn walk(self, _context: &mut WalkContext) -> Value {
        Value::from_type(self.type_)
    }
}

impl<'a> ReturnWalk<&mut WalkContext<'a>, symtab::Variable> for VariableDeclarationTree {
    fn walk(self, context: &mut WalkContext) -> symtab::Variable {
        let variable_type = match self.type_ {
            Some(type_tree) => Some(type_tree.walk(context).type_),
            None => None,
        };

        symtab::Variable::new(self.name.clone(), variable_type)
    }
}

impl<'a> ReturnWalk<&mut WalkContext<'a>, symtab::Variable> for ArgumentDeclarationTree {
    fn walk(self, context: &mut WalkContext) -> symtab::Variable {
        symtab::Variable::new(self.name.clone(), Some(self.type_.walk(context).type_))
    }
}

impl<'a> ReturnWalk<&mut WalkContext<'a>, symtab::FunctionPrototype> for FunctionDeclarationTree {
    fn walk(self, context: &mut WalkContext) -> symtab::FunctionPrototype {
        symtab::FunctionPrototype::new(
            self.name,
            self.arguments
                .into_iter()
                .map(|x| x.walk(context))
                .collect(),
            match self.return_type {
                Some(type_) => type_.walk(context).type_,
                None => Type::Unit,
            },
        )
    }
}

impl Walk for ArithmeticExpressionTree {
    fn walk(self, context: &mut WalkContext) -> Value {
        let (temp_dest, op) = match self {
            ArithmeticExpressionTree::Add(operands) => {
                let lhs: ir::Operand = operands.e1.walk(context).into();
                let rhs: ir::Operand = operands.e2.walk(context).into();
                let dest = context.next_temp(lhs.type_(context).clone());
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_add(dest, lhs, rhs),
                )
            }
            ArithmeticExpressionTree::Subtract(operands) => {
                let lhs: ir::Operand = operands.e1.walk(context).into();
                let rhs: ir::Operand = operands.e2.walk(context).into();
                let dest: ir::Operand = context.next_temp(lhs.type_(context).clone());
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_subtract(dest, lhs, rhs),
                )
            }
            ArithmeticExpressionTree::Multiply(operands) => {
                let lhs: ir::Operand = operands.e1.walk(context).into();
                let rhs: ir::Operand = operands.e2.walk(context).into();
                let dest = context.next_temp(lhs.type_(context).clone());
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_multiply(dest, lhs, rhs),
                )
            }
            ArithmeticExpressionTree::Divide(operands) => {
                let lhs: ir::Operand = operands.e1.walk(context).into();
                let rhs: ir::Operand = operands.e2.walk(context).into();
                let dest = context.next_temp(lhs.type_(context).clone());
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_divide(dest, lhs, rhs),
                )
            }
        };

        // TODO: associate location with arithmetic expression trees
        let operation = IrLine::new_binary_op(SourceLoc::none(), op);
        context.append_statement_to_current_block(operation);
        Value::from_operand(temp_dest, context)
    }
}

impl Walk for ComparisonExpressionTree {
    fn walk(self, context: &mut WalkContext) -> Value {
        let (temp_dest, op) = match self {
            ComparisonExpressionTree::LThan(operands) => {
                let lhs: ir::Operand = operands.e1.walk(context).into();
                let rhs: ir::Operand = operands.e2.walk(context).into();
                let dest = context.next_temp(lhs.type_(context).clone());
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_lthan(dest, lhs, rhs),
                )
            }
            ComparisonExpressionTree::GThan(operands) => {
                let lhs: ir::Operand = operands.e1.walk(context).into();
                let rhs: ir::Operand = operands.e2.walk(context).into();
                let dest = context.next_temp(lhs.type_(context).clone());
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_gthan(dest, lhs, rhs),
                )
            }
            ComparisonExpressionTree::LThanE(operands) => {
                let lhs: ir::Operand = operands.e1.walk(context).into();
                let rhs: ir::Operand = operands.e2.walk(context).into();
                let dest = context.next_temp(lhs.type_(context).clone());
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_lthan_e(dest, lhs, rhs),
                )
            }
            ComparisonExpressionTree::GThanE(operands) => {
                let lhs: ir::Operand = operands.e1.walk(context).into();
                let rhs: ir::Operand = operands.e2.walk(context).into();
                let dest = context.next_temp(lhs.type_(context).clone());
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_gthan_e(dest, lhs, rhs),
                )
            }
            ComparisonExpressionTree::Equals(operands) => {
                let lhs: ir::Operand = operands.e1.walk(context).into();
                let rhs: ir::Operand = operands.e2.walk(context).into();
                let dest = context.next_temp(lhs.type_(context).clone());
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_equals(dest, lhs, rhs),
                )
            }
            ComparisonExpressionTree::NotEquals(operands) => {
                let lhs: ir::Operand = operands.e1.walk(context).into();
                let rhs: ir::Operand = operands.e2.walk(context).into();
                let dest = context.next_temp(lhs.type_(context).clone());
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_not_equals(dest, lhs, rhs),
                )
            }
        };

        // TODO: association location with comparison expression tree
        let operation = IrLine::new_binary_op(SourceLoc::none(), op);
        context.append_statement_to_current_block(operation);
        Value::from_operand(temp_dest, context)
    }
}

impl Walk for ExpressionTree {
    fn walk(self, context: &mut WalkContext) -> Value {
        match self.expression {
            Expression::Identifier(ident) => {
                Value::from_operand(ir::Operand::new_as_variable(ident), context)
            }
            Expression::UnsignedDecimalConstant(constant) => Value::from_operand(
                ir::Operand::new_as_unsigned_decimal_constant(constant),
                context,
            ),
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
                context.append_statement_to_current_block(field_read_line);
                Value::from_operand(destination, context)
            }
            Expression::MethodCall(method_call) => method_call.walk(context),
        }
    }
}

impl Walk for AssignmentTree {
    fn walk(self, context: &mut WalkContext) -> Value {
        let assignment_ir = match self.assignee.expression {
            Expression::FieldExpression(field_expression_tree) => {
                let (receiver, field) = field_expression_tree.walk(context);
                IrLine::new_field_write(
                    self.value.walk(context).into(),
                    self.loc,
                    receiver.into(),
                    field.operand.unwrap().get_name().unwrap().base_name.clone(),
                )
            }
            _ => IrLine::new_assignment(
                self.loc,
                self.assignee.walk(context).into(),
                self.value.walk(context).into(),
            ),
        };

        context.append_statement_to_current_block(assignment_ir);

        Value::unit()
    }
}

impl Walk for IfExpressionTree {
    fn walk(self, context: &mut WalkContext) -> Value {
        // FUTURE: optimize condition walk to use different jumps
        let condition_loc = self.condition.loc;
        let condition_result: ir::Operand = self.condition.walk(context).into();
        let if_condition = ir::JumpCondition::NE(ir::operands::DualSourceOperands::new(
            condition_result,
            ir::Operand::new_as_unsigned_decimal_constant(0),
        ));

        let (_, maybe_else_label) =
            context.create_conditional_branch_from_current(condition_loc, if_condition);
        let if_value = self.true_block.walk(context);

        // create a separate, mutable value which contains the true result
        let mut result_value = if_value.clone();

        // if a false block exists AND the 'if' value exists
        if self.false_block.is_some() && if_value.operand.is_some() {
            // we need to copy the 'if' result to the common result_value at the end of the 'if' block
            let result_operand = context.next_temp(if_value.type_.clone());
            result_value = Value::from_operand(result_operand.clone(), context);
            let assign_if_result_line =
                ir::IrLine::new_assignment(self.loc, result_operand, if_value.clone().into());
            context.append_statement_to_current_block(assign_if_result_line);
        }
        context.converge_current_block();

        match (maybe_else_label, self.false_block) {
            (Some(else_label), Some(else_block)) => {
                context.set_current_block(else_label);
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
                    context.append_statement_to_current_block(assign_else_result_line);
                }

                context.converge_current_block();
            }
            (None, None) => {}
            (_, _) => {
                panic!(
                    "Mismatched else label and else block - expect to have either both or neither"
                );
            }
        };

        result_value
    }
}

impl Walk for WhileExpressionTree {
    fn walk(self, context: &mut WalkContext) -> Value {
        let loop_done = context.create_loop(self.loc, self.condition);

        self.body.walk(context);
        context.converge_current_block();
        context.set_current_block(loop_done);

        Value::unit()
    }
}

// returns (receiver, field_info)
// receiver is the value containing the receiver of the field access
// field_info is a value containing name and type information of the field being accessed
impl<'a> ReturnWalk<&mut WalkContext<'a>, (Value, Value)> for FieldExpressionTree {
    fn walk(self, context: &mut WalkContext) -> (Value, Value) {
        let receiver = self.receiver.walk(context);

        let struct_name = match &receiver.type_ {
            Type::UDT(type_name) => type_name,
            _ => panic!(
                "Field expression receiver must be of struct type (got {})",
                receiver.type_
            ),
        };

        let receiver_definition = context.lookup_struct(struct_name).expect(&format!(
            "Error handling for failed lookups is unimplemented: {}.{}",
            receiver.type_, self.field
        ));
        // TODO: error handling
        let accessed_field_type = receiver_definition.get_field_type(&self.field).unwrap();
        (
            receiver,
            Value::from_type_and_name(
                accessed_field_type.clone(),
                ir::Operand::Variable(ir::OperandName::new_basic(self.field)),
            ),
        )
    }
}

impl<'a> ReturnWalk<&mut WalkContext<'a>, Vec<Value>> for CallParamsTree {
    fn walk(self, context: &mut WalkContext) -> Vec<Value> {
        let mut param_values = Vec::new();

        for param in self.params {
            param_values.push(param.walk(context));
        }

        param_values
    }
}

impl Walk for MethodCallExpressionTree {
    fn walk(self, context: &mut WalkContext) -> Value {
        let receiver = self.receiver.walk(context);

        let type_definition = context.lookup_type(&receiver.type_).unwrap();
        let called_method: symtab::FunctionPrototype = type_definition
            .lookup_method(&self.called_method)
            .unwrap()
            .prototype()
            .clone();

        let return_value_to = if called_method.return_type != Type::Unit {
            Some(context.next_temp(called_method.return_type))
        } else {
            None
        };
        // //TODO: error handling and checking
        // assert!(called_method.arguments.len() == params.len());

        let params: Vec<ir::Operand> = self
            .params
            .walk(context)
            .into_iter()
            .map(|value| value.into())
            .collect();

        let method_call_line = IrLine::new_method_call(
            self.loc,
            receiver.into(),
            &called_method.name,
            params,
            return_value_to.clone(),
        );

        context.append_statement_to_current_block(method_call_line);

        match return_value_to {
            Some(operand) => Value::from_operand(operand, context),
            None => Value::unit(),
        }
    }
}

impl Walk for StatementTree {
    fn walk(self, context: &mut WalkContext) -> Value {
        match self.statement {
            Statement::VariableDeclaration(declaration_tree) => {
                let declared_variable = declaration_tree.walk(context);
                context.scope().insert_variable(declared_variable);
                Value::unit()
            }
            Statement::Expression(expression_tree) => expression_tree.walk(context),
        }
    }
}

impl Walk for CompoundExpressionTree {
    fn walk(mut self, context: &mut WalkContext) -> Value {
        context.push_scope(symtab::Scope::new());
        let last_statement = self.statements.pop();
        for statement in self.statements {
            statement.walk(context);
        }

        let last_statement_value = match last_statement {
            Some(statement_tree) => match statement_tree.statement {
                Statement::VariableDeclaration(variable_declaration_tree) => {
                    variable_declaration_tree.walk(context);
                    Value::unit()
                }
                Statement::Expression(expression_tree) => expression_tree.walk(context),
            },
            None => Value::unit(),
        };

        context.pop_scope_to_subscope_of_next();

        last_statement_value
    }
}
