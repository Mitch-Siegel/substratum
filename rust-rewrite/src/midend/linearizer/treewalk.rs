use crate::{
    frontend::{ast::*, sourceloc::SourceLoc},
    midend::{
        ir::{self, IrLine},
        symtab::{self, SymbolTable},
        types::Type,
    },
};

use super::walkcontext::WalkContext;

pub trait TableWalk {
    fn walk(self, symbol_table: &mut SymbolTable);
}

pub trait ReturnWalk<T> {
    fn walk(self) -> T;
}

pub trait ContextWalk {
    fn walk(self, context: &mut WalkContext);
}

pub trait OperandWalk {
    fn walk(self, loc: SourceLoc, context: &mut WalkContext) -> ir::Operand;
}

impl TableWalk for TranslationUnitTree {
    fn walk(self, symbol_table: &mut SymbolTable) {
        match self.contents {
            TranslationUnit::FunctionDeclaration(tree) => {
                let declared_function = tree.walk();
                symbol_table.insert_function_prototype(declared_function);
            }
            TranslationUnit::FunctionDefinition(tree) => {
                let mut declared_prototype = tree.prototype.walk();
                let mut context = WalkContext::new();
                context.push_scope(declared_prototype.create_argument_scope());

                tree.body.walk(&mut context);
                let argument_scope = context.pop_last_scope();

                symbol_table.insert_function(symtab::Function::new(
                    declared_prototype,
                    argument_scope,
                    context.take_control_flow(),
                ));
            }
        }
    }
}

impl ReturnWalk<Type> for TypenameTree {
    fn walk(self) -> Type {
        match self.name.as_str() {
            "u8" => Type::new_u8(0),
            "u16" => Type::new_u16(0),
            "u32" => Type::new_u32(0),
            "u64" => Type::new_u64(0),
            _ => {
                panic!("impossible type!");
            }
        }
    }
}

impl ReturnWalk<symtab::Variable> for VariableDeclarationTree {
    fn walk(self) -> symtab::Variable {
        symtab::Variable::new(self.name.clone(), self.typename.walk())
    }
}

impl ReturnWalk<symtab::FunctionPrototype> for FunctionDeclarationTree {
    fn walk(self) -> symtab::FunctionPrototype {
        symtab::FunctionPrototype::new(
            self.name,
            self.arguments.into_iter().map(|x| x.walk()).collect(),
            match self.return_type {
                Some(typename) => Some(typename.walk()),
                None => None,
            },
        )
    }
}

impl OperandWalk for ArithmeticOperationTree {
    fn walk(self, loc: SourceLoc, context: &mut WalkContext) -> ir::Operand {
        let (temp_dest, op) = match self {
            ArithmeticOperationTree::Add(operands) => {
                let lhs = operands.e1.walk(loc, context);
                let rhs = operands.e2.walk(loc, context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_add(dest, lhs, rhs),
                )
            }
            ArithmeticOperationTree::Subtract(operands) => {
                let lhs = operands.e1.walk(loc, context);
                let rhs = operands.e2.walk(loc, context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_subtract(dest, lhs, rhs),
                )
            }
            ArithmeticOperationTree::Multiply(operands) => {
                let lhs = operands.e1.walk(loc, context);
                let rhs = operands.e2.walk(loc, context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_multiply(dest, lhs, rhs),
                )
            }
            ArithmeticOperationTree::Divide(operands) => {
                let lhs = operands.e1.walk(loc, context);
                let rhs = operands.e2.walk(loc, context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_divide(dest, lhs, rhs),
                )
            }
        };

        let operation = IrLine::new_binary_op(loc, op);
        context.append_statement_to_current_block(operation);
        temp_dest
    }
}

impl OperandWalk for ComparisonOperationTree {
    fn walk(self, loc: SourceLoc, context: &mut WalkContext) -> ir::Operand {
        let (temp_dest, op) = match self {
            ComparisonOperationTree::LThan(operands) => {
                let lhs = operands.e1.walk(loc, context);
                let rhs = operands.e2.walk(loc, context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_lthan(dest, lhs, rhs),
                )
            }
            ComparisonOperationTree::GThan(operands) => {
                let lhs = operands.e1.walk(loc, context);
                let rhs = operands.e2.walk(loc, context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_gthan(dest, lhs, rhs),
                )
            }
            ComparisonOperationTree::LThanE(operands) => {
                let lhs = operands.e1.walk(loc, context);
                let rhs = operands.e2.walk(loc, context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_lthan_e(dest, lhs, rhs),
                )
            }
            ComparisonOperationTree::GThanE(operands) => {
                let lhs = operands.e1.walk(loc, context);
                let rhs = operands.e2.walk(loc, context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_gthan_e(dest, lhs, rhs),
                )
            }
            ComparisonOperationTree::Equals(operands) => {
                let lhs = operands.e1.walk(loc, context);
                let rhs = operands.e2.walk(loc, context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_equals(dest, lhs, rhs),
                )
            }
            ComparisonOperationTree::NotEquals(operands) => {
                let lhs = operands.e1.walk(loc, context);
                let rhs = operands.e2.walk(loc, context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_not_equals(dest, lhs, rhs),
                )
            }
        };
        let operation = IrLine::new_binary_op(loc, op);
        context.append_statement_to_current_block(operation);
        temp_dest
    }
}

impl OperandWalk for ExpressionTree {
    fn walk(self, _loc: SourceLoc, context: &mut WalkContext) -> ir::Operand {
        match self.expression {
            Expression::Identifier(ident) => ir::Operand::new_as_variable(ident),
            Expression::UnsignedDecimalConstant(constant) => {
                ir::Operand::new_as_unsigned_decimal_constant(constant)
            }
            Expression::Arithmetic(arithmetic_operation) => {
                arithmetic_operation.walk(self.loc, context)
            }
            Expression::Comparison(comparison_operation) => {
                comparison_operation.walk(self.loc, context)
            }
        }
    }
}

impl ContextWalk for AssignmentTree {
    fn walk(self, context: &mut WalkContext) {
        let assignment_ir = IrLine::new_assignment(
            self.loc,
            ir::Operand::new_as_variable(self.identifier),
            self.value.walk(self.loc, context),
        );
        context.append_statement_to_current_block(assignment_ir);
    }
}

impl ContextWalk for IfStatementTree {
    fn walk(self, context: &mut WalkContext) {
        // FUTURE: optimize condition walk to use different jumps
        let condition_loc = self.condition.loc.clone();
        let condition_result = self.condition.walk(condition_loc, context);
        let if_condition = ir::JumpCondition::NE(ir::operands::DualSourceOperands::new(
            condition_result,
            ir::Operand::new_as_unsigned_decimal_constant(0),
        ));

        let (_, maybe_else_label) =
            context.create_conditional_branch_from_current(condition_loc, if_condition);
        self.true_block.walk(context);
        context.converge_current_block();

        match (maybe_else_label, self.false_block) {
            (Some(else_label), Some(else_block)) => {
                context.set_current_block(else_label);
                else_block.walk(context);
                context.converge_current_block();
            }
            (None, None) => {}
            (_, _) => {
                panic!(
                    "Mismatched else label and else block - expect to have either both or neither"
                );
            }
        };
    }
}

impl ContextWalk for WhileLoopTree {
    fn walk(self, context: &mut WalkContext) {
        let loop_done = context.create_loop(self.loc, self.condition);

        self.body.walk(context);
        context.converge_current_block();
        context.set_current_block(loop_done);
    }
}

impl ContextWalk for StatementTree {
    fn walk(self, context: &mut WalkContext) {
        match self.statement {
            Statement::VariableDeclaration(tree) => context.scope().insert_variable(tree.walk()),
            Statement::Assignment(tree) => tree.walk(context),
            Statement::IfStatement(tree) => tree.walk(context),
            Statement::WhileLoop(tree) => tree.walk(context),
        }
    }
}

impl ContextWalk for CompoundStatementTree {
    fn walk(self, context: &mut WalkContext) {
        context.push_scope(symtab::Scope::new());
        for statement in self.statements {
            statement.walk(context);
        }
        context.pop_scope_to_subscope_of_next();
    }
}
