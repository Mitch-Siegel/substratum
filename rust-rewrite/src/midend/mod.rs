pub mod basic_block;
pub mod control_flow;
mod idfa;
pub mod ir;
pub mod program_point;
pub mod symtab;
pub mod types;

use control_flow::ControlFlow;
use ir::*;
pub use symtab::*;
use types::*;

use crate::frontend::{ast::*, sourceloc::*};

struct WalkContext {
    control_flow: ControlFlow,
    branch_points: Vec<usize>,
    convergence_points: Vec<usize>,
    scopes: Vec<Scope>,
}

impl WalkContext {
    fn new() -> WalkContext {
        let mut starter_flow = ControlFlow::new();
        starter_flow.next_block();
        starter_flow.next_block();
        WalkContext {
            control_flow: starter_flow,
            branch_points: vec![0],
            convergence_points: vec![1],
            scopes: Vec::new(),
        }
    }

    pub fn take_control_flow(mut self) -> ControlFlow {
        match self.convergence_points.last() {
            Some(1) => {
                self.finish_branch_and_finalize_convergence();
            }
            _ => {}
        }

        assert!(self.branch_points.len() == 0);
        assert!(self.convergence_points.len() == 0);
        self.control_flow
    }

    pub fn append_to_current_block(&mut self, statement: IR) {
        self.control_flow
            .append_statement_to_current_block(statement);
    }

    pub fn append_to_block(&mut self, statement: IR, block: usize) {
        self.control_flow
            .append_statement_to_block(statement, block);
    }

    fn create_branching_point(&mut self) {
        self.branch_points.push(self.control_flow.current_block());
        println!(
            "Create branching point from {}",
            self.branch_points.last().unwrap()
        );
    }

    fn create_convergence_point(&mut self) {
        self.convergence_points
            .push(self.control_flow.next_block().label());
        println!(
            "Create branching point to {}",
            self.convergence_points.last().unwrap()
        );
    }

    pub fn create_branching_point_with_convergence(&mut self) {
        self.create_branching_point();
        self.create_convergence_point();
    }

    pub fn create_branch(
        &mut self,
        loc: SourceLoc,
        condition: ir::operands::JumpCondition<String>,
    ) {
        assert!(self.branch_points.len() > 0);
        assert!(self.convergence_points.len() > 0);
        assert!(*self.branch_points.last().unwrap() == self.control_flow.current_block());

        let branch_target = self.control_flow.next_block().label();

        println!(
            "Create branch from {}->{}",
            self.control_flow.current_block(),
            branch_target
        );

        let branch_ir = IR::new_jump(loc, branch_target, condition);
        self.append_to_current_block(branch_ir);
        self.control_flow.set_current_block(branch_target);
    }

    // assuming the control flow is branched from a branch point with a convergence block set up
    // append an unconditional jump from the end of the current block to the convergence block
    // then, set the current block back to the branch point
    pub fn finish_branch(&mut self) {
        let converge_to = *self
            .convergence_points
            .last()
            .expect("WalkContext::converge_branch expects valid convergence point");

        println!(
            "Finish branch (converge from {}->{})",
            self.control_flow.current_block(),
            converge_to
        );

        let convergence_jump = IR::new_jump(
            SourceLoc::none(),
            converge_to,
            ir::operands::JumpCondition::<String>::Unconditional,
        );
        self.append_to_current_block(convergence_jump);

        self.control_flow.set_current_block(
            *self
                .branch_points
                .last()
                .expect("WalkContext::finish_branch expects valid branch point to return to"),
        );
    }

    pub fn finish_branch_and_finalize_convergence(&mut self) {
        let converge_to = self.convergence_points.pop().expect(
            "WalkContext::finish_branch_and_finalize_convergence expects valid convergence point",
        );
        assert!(self.branch_points.pop().is_some());

        println!(
            "Finish branch and finalize convergence (converge from {}->{} - current block now {})",
            self.control_flow.current_block(),
            converge_to,
            converge_to
        );

        let convergence_jump = IR::new_jump(
            SourceLoc::none(),
            converge_to,
            ir::operands::JumpCondition::<String>::Unconditional,
        );

        self.append_to_current_block(convergence_jump);

        self.control_flow.set_current_block(converge_to);
    }

    pub fn create_loop(
        &mut self,
        loc: SourceLoc,
        condition: ExpressionTree,
        body: CompoundStatementTree,
    ) {
        let loop_top = self.next_block(loc);
        let after_loop = self.control_flow.next_block().label();
        self.control_flow.set_current_block(loop_top);

        // FUTURE: optimize condition handling to use different jumps
        let condition_result = condition.walk(self);
        let loop_false_condition =
            ir::operands::JumpCondition::<String>::Eq(ir::operands::DualSourceOperands::from(
                condition_result,
                BasicOperand::new_as_unsigned_decimal_constant(0),
            ));

        let loop_false_jump = IR::new_jump(loc, after_loop, loop_false_condition);
        self.append_to_current_block(loop_false_jump);

        body.walk(self);
        let looping_jump = IR::new_jump(
            loc,
            loop_top,
            ir::operands::JumpCondition::<String>::Unconditional,
        );
        self.append_to_current_block(looping_jump);

        self.control_flow.set_current_block(after_loop);
    }

    pub fn next_temp(&mut self, type_: Type) -> ir::operands::BasicOperand {
        let temp_name = self.control_flow.next_temp();
        self.scope()
            .insert_variable(Variable::new(temp_name.clone(), type_));
        ir::BasicOperand::new_as_temporary(temp_name)
    }

    // finishes the current block, adds a jump to a new block, and sets that new block as the current
    pub fn next_block(&mut self, loc: SourceLoc) -> usize {
        let new_label = self.control_flow.next_block().label();

        let exit_jump = IR::new_jump(
            loc,
            new_label,
            ir::operands::JumpCondition::<String>::Unconditional,
        );
        self.append_to_current_block(exit_jump);

        self.control_flow.set_current_block(new_label);
        new_label
    }

    pub fn push_scope(&mut self, scope: Scope) {
        self.scopes.push(scope)
    }

    pub fn pop_scope_to_subscope_of_next(&mut self) {
        let popped = self
            .scopes
            .pop()
            .expect("WalkContext::pop_scope_to_subscope_of_next expects valid scope");
        self.scope().insert_subscope(popped);
    }

    pub fn pop_last_scope(&mut self) -> Scope {
        if self.scopes.len() > 1 {
            panic!(
                "WalkContext::pop_last_scope() called with {} parent scopes",
                self.scopes.len()
            );
        }

        self.scopes
            .pop()
            .expect("WalkContext::pop_last_scope() called with no scopese")
    }

    pub fn scope(&mut self) -> &mut Scope {
        self.scopes
            .last_mut()
            .expect("WalkContext::scope() expects valid scope")
    }

    pub fn lookup_variable_by_name(&self, name: &str) -> Option<&Variable> {
        for scope in (&self.scopes).into_iter().rev().by_ref() {
            match scope.lookup_variable_by_name(name) {
                Some(variable) => return Some(variable),
                None => {}
            }
        }
        None
    }
}

pub trait TableWalk {
    fn walk(self, symbol_table: &mut SymbolTable);
}

trait ScopeWalk {
    fn walk(&self, scope: &mut Scope);
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

                symbol_table.insert_function(Function::new(
                    declared_prototype,
                    argument_scope,
                    context.take_control_flow(),
                ));
            }
        }
    }
}

impl TypenameTree {
    fn walk(&self) -> Type {
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

impl VariableDeclarationTree {
    fn walk(&self) -> Variable {
        Variable::new(self.name.clone(), self.typename.walk())
    }
}

impl FunctionDeclarationTree {
    fn walk(self) -> FunctionPrototype {
        FunctionPrototype::new(
            self.name,
            self.arguments.into_iter().map(|x| x.walk()).collect(),
            match self.return_type {
                Some(typename) => Some(typename.walk()),
                None => None,
            },
        )
    }
}

impl ArithmeticOperationTree {
    pub fn walk(self, loc: SourceLoc, context: &mut WalkContext) -> ir::operands::BasicOperand {
        let (temp_dest, op) = match self {
            ArithmeticOperationTree::Add(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_add(dest, lhs, rhs),
                )
            }
            ArithmeticOperationTree::Subtract(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_divide(dest, lhs, rhs),
                )
            }
            ArithmeticOperationTree::Multiply(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_multiply(dest, lhs, rhs),
                )
            }
            ArithmeticOperationTree::Divide(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_divide(dest, lhs, rhs),
                )
            }
        };

        let operation = IR::new_binary_op(loc, op);
        context.append_to_current_block(operation);
        temp_dest
    }
}

impl ComparisonOperationTree {
    pub fn walk(self, loc: SourceLoc, context: &mut WalkContext) -> ir::operands::BasicOperand {
        let (temp_dest, op) = match self {
            ComparisonOperationTree::LThan(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_lthan(dest, lhs, rhs),
                )
            }
            ComparisonOperationTree::GThan(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_gthan(dest, lhs, rhs),
                )
            }
            ComparisonOperationTree::LThanE(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_lthan_e(dest, lhs, rhs),
                )
            }
            ComparisonOperationTree::GThanE(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_gthan_e(dest, lhs, rhs),
                )
            }
            ComparisonOperationTree::Equals(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_equals(dest, lhs, rhs),
                )
            }
            ComparisonOperationTree::NotEquals(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (
                    dest.clone(),
                    ir::operations::BinaryOperations::new_not_equals(dest, lhs, rhs),
                )
            }
        };
        let operation = IR::new_binary_op(loc, op);
        context.append_to_current_block(operation);
        temp_dest
    }
}

impl ExpressionTree {
    pub fn walk(self, context: &mut WalkContext) -> ir::operands::BasicOperand {
        match self.expression {
            Expression::Identifier(ident) => ir::operands::BasicOperand::new_as_variable(ident),
            Expression::UnsignedDecimalConstant(constant) => {
                ir::operands::BasicOperand::new_as_unsigned_decimal_constant(constant)
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

impl AssignmentTree {
    pub fn walk(self, context: &mut WalkContext) {
        let assignment_ir = IR::new_assignment(
            self.loc,
            ir::operands::BasicOperand::new_as_variable(self.identifier),
            self.value.walk(context),
        );
        context.append_to_current_block(assignment_ir);
    }
}

impl IfStatementTree {
    fn walk(self, context: &mut WalkContext) {
        // FUTURE: optimize condition walk to use different jumps
        let condition_result = self.condition.walk(context);
        let if_condition =
            ir::operands::JumpCondition::<String>::NE(ir::operands::DualSourceOperands::from(
                condition_result,
                BasicOperand::new_as_unsigned_decimal_constant(0),
            ));

        context.create_branching_point_with_convergence();
        context.create_branch(self.true_block.loc, if_condition);
        self.true_block.walk(context);
        context.finish_branch();

        match self.false_block {
            Some(false_block) => {
                context.create_branch(false_block.loc, ir::operands::JumpCondition::Unconditional);
                false_block.walk(context);
            }
            None => {}
        }
        context.finish_branch_and_finalize_convergence();
    }
}

impl WhileLoopTree {
    fn walk(self, context: &mut WalkContext) {
        context.create_loop(self.loc, self.condition, self.body);
    }
}

impl StatementTree {
    fn walk(self, context: &mut WalkContext) {
        match self.statement {
            Statement::VariableDeclaration(tree) => context.scope().insert_variable(tree.walk()),
            Statement::Assignment(tree) => tree.walk(context),
            Statement::IfStatement(tree) => tree.walk(context),
            Statement::WhileLoop(tree) => tree.walk(context),
        }
    }
}

impl CompoundStatementTree {
    fn walk(self, context: &mut WalkContext) {
        context.push_scope(Scope::new());
        for statement in self.statements {
            statement.walk(context);
        }
        context.pop_scope_to_subscope_of_next();
    }
}
