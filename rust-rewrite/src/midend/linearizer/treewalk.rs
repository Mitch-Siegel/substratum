use crate::{
    frontend::{ast::*, sourceloc::SourceLoc},
    midend::{
        ir::{self, IrLine},
        linearizer::*,
        symtab::{self, FunctionOwner, MethodOwner, TypeOwner, VariableOwner},
        types::Type,
    },
};

use name_derive::NameReflectable;

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

    pub fn from_operand(operand: ir::Operand, context: &FunctionWalkContext) -> Self {
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

pub trait ModuleWalk {
    fn walk(self, context: &mut ModuleWalkContext);
}

pub trait FunctionWalk {
    fn walk(self, context: &mut FunctionWalkContext) -> Value;
}

pub trait ReturnWalk<T, U> {
    fn walk(self, context: T) -> U;
}

impl ModuleWalk for TranslationUnitTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut ModuleWalkContext) {
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
                context.insert_function(defined_function).unwrap();
            }
            TranslationUnit::StructDefinition(struct_definition) => {
                let struct_repr = struct_definition.walk(context);
                context
                    .insert_type(symtab::TypeDefinition::new(
                        Type::UDT(struct_repr.name.clone()),
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

impl ModuleWalk for ImplementationTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut ModuleWalkContext) {
        let implemented_for_type = self.type_.walk(context).type_;

        let methods: Vec<symtab::Function> = self
            .items
            .into_iter()
            .map(|item| item.walk(context).unwrap())
            .collect();

        let implemented_for_mut = context.lookup_type_mut(&implemented_for_type).unwrap();

        for method in methods {
            implemented_for_mut.insert_method(method).unwrap();
        }
    }
}

impl ReturnWalk<&mut ModuleWalkContext, Result<symtab::Function, symtab::SymbolError>>
    for FunctionDefinitionTree
{
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(
        self,
        context: &mut ModuleWalkContext,
    ) -> Result<symtab::Function, symtab::SymbolError> {
        let declared_prototype = self.prototype.walk(context);
        let mut function_context = FunctionWalkContext::new(context, declared_prototype, None);

        self.body.walk(&mut function_context);
        Ok(function_context.into())
    }
}

impl<T> ReturnWalk<&T, Value> for TypeTree
where
    T: symtab::ModuleOwnerships,
{
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &T) -> Value {
        // check that the type exists by looking it up
        context.lookup_type(&self.type_).unwrap();
        Value::from_type(self.type_)
    }
}

impl<T> ReturnWalk<&T, symtab::Variable> for VariableDeclarationTree
where
    T: symtab::ModuleOwnerships,
{
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &T) -> symtab::Variable {
        let variable_type = match self.type_ {
            Some(type_tree) => Some(type_tree.walk(context).type_),
            None => None,
        };

        symtab::Variable::new(self.name.clone(), variable_type)
    }
}

impl<T> ReturnWalk<&T, symtab::Variable> for ArgumentDeclarationTree
where
    T: symtab::ModuleOwnerships,
{
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &T) -> symtab::Variable {
        symtab::Variable::new(self.name.clone(), Some(self.type_.walk(context).type_))
    }
}

impl<'a> ReturnWalk<&mut ModuleWalkContext, symtab::FunctionPrototype> for FunctionDeclarationTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut ModuleWalkContext) -> symtab::FunctionPrototype {
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

impl ReturnWalk<&ModuleWalkContext, symtab::StructRepr> for StructDefinitionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &ModuleWalkContext) -> symtab::StructRepr {
        let fields = self
            .fields
            .into_iter()
            .map(|field| {
                let field_type = field.type_.walk(context).type_;
                (field.name, field_type)
            })
            .collect::<Vec<_>>();
        symtab::StructRepr::new(self.name, fields, context).unwrap()
    }
}

impl FunctionWalk for ArithmeticExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Value {
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
        context
            .append_statement_to_current_block(operation)
            .unwrap();
        Value::from_operand(temp_dest, context)
    }
}

impl FunctionWalk for ComparisonExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Value {
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
        context
            .append_statement_to_current_block(operation)
            .unwrap();
        Value::from_operand(temp_dest, context)
    }
}

impl FunctionWalk for ExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Value {
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
                context
                    .append_statement_to_current_block(field_read_line)
                    .unwrap();
                Value::from_operand(destination, context)
            }
            Expression::MethodCall(method_call) => method_call.walk(context),
        }
    }
}

impl FunctionWalk for AssignmentTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Value {
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

        context
            .append_statement_to_current_block(assignment_ir)
            .unwrap();

        Value::unit()
    }
}

impl FunctionWalk for IfExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Value {
        // FUTURE: optimize condition walk to use different jumps
        let condition_loc = self.condition.loc;
        let condition_result: ir::Operand = self.condition.walk(context).into();
        let if_condition = ir::JumpCondition::NE(ir::operands::DualSourceOperands::new(
            condition_result,
            ir::Operand::new_as_unsigned_decimal_constant(0),
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
            let result_operand = context.next_temp(if_value.type_.clone());
            result_value = Value::from_operand(result_operand.clone(), context);
            let assign_if_result_line =
                ir::IrLine::new_assignment(self.loc, result_operand, if_value.clone().into());
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

impl FunctionWalk for WhileExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Value {
        let loop_done_label = context.create_loop(self.loc);

        self.body.walk(context);

        Value::unit()
    }
}

// returns (receiver, field_info)
// receiver is the value containing the receiver of the field access
// field_info is a value containing name and type information of the field being accessed
impl<'a> ReturnWalk<&mut FunctionWalkContext<'a>, (Value, Value)> for FieldExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> (Value, Value) {
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

        let accessed_field = receiver_definition.lookup_field(&self.field).unwrap();
        (
            receiver,
            Value::from_type_and_name(
                accessed_field.type_.clone(),
                ir::Operand::Variable(ir::OperandName::new_basic(self.field)),
            ),
        )
    }
}

impl<'a> ReturnWalk<&mut FunctionWalkContext<'a>, Vec<Value>> for CallParamsTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Vec<Value> {
        let mut param_values = Vec::new();

        for param in self.params {
            param_values.push(param.walk(context));
        }

        param_values
    }
}

impl FunctionWalk for MethodCallExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Value {
        let receiver = self.receiver.walk(context);

        let type_definition = context.lookup_type(&receiver.type_).unwrap();
        let called_method: symtab::FunctionPrototype = type_definition
            .lookup_method(&self.called_method)
            .unwrap()
            .prototype
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

        context
            .append_statement_to_current_block(method_call_line)
            .unwrap();

        match return_value_to {
            Some(operand) => Value::from_operand(operand, context),
            None => Value::unit(),
        }
    }
}

impl FunctionWalk for StatementTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Value {
        match self.statement {
            Statement::VariableDeclaration(declaration_tree) => {
                let declared_variable = declaration_tree.walk(context);
                context.insert_variable(declared_variable).unwrap();
                Value::unit()
            }
            Statement::Expression(expression_tree) => expression_tree.walk(context),
        }
    }
}

impl FunctionWalk for CompoundExpressionTree {
    #[tracing::instrument(skip(self, context), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(mut self, context: &mut FunctionWalkContext) -> Value {
        context.unconditional_branch_from_current(self.loc).unwrap();

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

        context.finish_branch().unwrap();

        last_statement_value
    }
}
