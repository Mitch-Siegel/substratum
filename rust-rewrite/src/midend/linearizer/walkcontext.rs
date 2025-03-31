use std::collections::{HashMap, HashSet};

use crate::{
    frontend::{ast, sourceloc::SourceLoc},
    midend::{ir, symtab, types::Type},
};

use super::treewalk::*;

pub struct WalkContext {
    control_flow: ir::ControlFlow,
    branch_points: HashMap<usize, HashSet<usize>>, // map from branch origin to set of target blocks
    convergence_points: HashMap<usize, usize>, // map of label -> label that block should jump to when done
    scopes: Vec<symtab::Scope>,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
    current_block: usize,
}

impl WalkContext {
    pub fn new() -> WalkContext {
        let mut starter_flow = ir::ControlFlow::new();
        WalkContext {
            control_flow: starter_flow,
            branch_points: HashMap::<usize, HashSet<usize>>::new(),
            convergence_points: HashMap::<usize, usize>::new(),
            scopes: Vec::new(),
            temp_num: 0,
            current_block: 0,
        }
    }

    pub fn take_control_flow(mut self) -> ir::ControlFlow {
        assert!(self.branch_points.len() == 0);
        assert!(self.convergence_points.len() == 0);
        self.control_flow
    }

    fn replace_branch_and_convergence_points(&mut self, old_block: usize, new_block: usize) {
        // replace all instances of old_block with new_block in both branch and convergence point tracking
        self.branch_points = self
            .branch_points
            .iter()
            .map(|(source, dest_set)| {
                (
                    if *source == old_block {
                        new_block
                    } else {
                        *source
                    },
                    dest_set
                        .into_iter()
                        .map(|target| {
                            if *target == old_block {
                                new_block
                            } else {
                                *target
                            }
                        })
                        .collect(),
                )
            })
            .collect();

        self.convergence_points = self
            .convergence_points
            .iter()
            .map(|(from, to)| {
                (
                    if *from == old_block { new_block } else { *from },
                    if *to == old_block { new_block } else { *to },
                )
            })
            .collect();
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IrLine) {
        match &statement.operation {
            ir::Operations::Jump(_) => {
                panic!("WalkContext::append_statement_to_current_block does NOT support jumps!")
            }
            _ => {}
        }
        self.append_to_current_block(statement);
    }

    // appends the given statement to the current basic block
    // if the statement is any sort of branch, the current block will be updated to be the target of the branch
    // if the branch is conditional, the function returns Some(false_label) where false_label is the target of the
    // block control flows to when the condition is not met
    // for unconditional branches and other statements, returns None
    fn append_to_current_block(&mut self, statement: ir::IrLine) -> Option<usize> {
        match self
            .control_flow
            .append_statement_to_block(statement, self.current_block)
        {
            (Some(new_current), false_label) => {
                self.replace_branch_and_convergence_points(self.current_block, new_current);
                self.set_current_block(new_current);
                false_label
            }
            (None, _) => None,
        }
    }

    // creates a branch from the current block based on a condition
    // returns (true_label, Some(false_label)) if branch is conditional, else (true_label, None)
    fn create_branch_from_current(&mut self) -> usize {
        let branch_target = self.control_flow.next_block();

        // TODO: re-examine to see if checking is required here
        self.branch_points
            .entry(self.current_block)
            .or_default()
            .insert(branch_target);
        branch_target
    }

    fn add_branch_from_current(&mut self, target: usize) {
        self.branch_points
            .entry(self.current_block)
            .or_default()
            .insert(target);
    }

    fn create_convergence_point_for_branch(&mut self, branch_from: usize) {
        let convergence_point = self.control_flow.next_block();

        for branch_target in self
            .branch_points
            .get(&branch_from)
            .expect("Creation of convergence points requires existence of branch")
        {
            match self
                .convergence_points
                .insert(*branch_target, convergence_point)
            {
                Some(existing_convergence) => panic!(
                    "Block {} already converges to block {}",
                    branch_target, existing_convergence
                ),
                None => {}
            }
        }
    }

    fn converge_block(&mut self, block_label: usize) {
        let converge_to = self
            .convergence_points
            .remove(&block_label)
            .expect(&format!("Block {} has no convergence point", block_label));

        let convergence_jump = ir::IrLine::new_jump(
            SourceLoc::none(),
            converge_to,
            ir::JumpCondition::Unconditional,
        );
        // ignore return value - appending an unconditional jump
        self.control_flow
            .append_statement_to_block(convergence_jump, block_label);
    }

    pub fn converge_current_block(&mut self) {
        let converge_to = self
            .convergence_points
            .remove(&self.current_block)
            .expect(&format!(
                "Block {} has no convergence point",
                self.current_block
            ));

        let convergence_jump = ir::IrLine::new_jump(
            SourceLoc::none(),
            converge_to,
            ir::JumpCondition::Unconditional,
        );
        // ignore return value - appending an unconditional jump
        self.append_to_current_block(convergence_jump);
    }

    pub fn set_current_block(&mut self, label: usize) {
        self.current_block = label;
    }

    pub fn create_if_statement(
        &mut self,
        loc: SourceLoc,
        condition: ir::JumpCondition,
    ) -> (usize, Option<usize>) {
        let if_label = self.create_branch_from_current();

        let if_condition_jump = ir::IrLine::new_jump(loc, if_label, condition);
        let else_label = self.append_to_current_block(if_condition_jump);

        (if_label, else_label)
    }

    pub fn create_loop(
        &mut self,
        loc: SourceLoc,
        condition: ast::ExpressionTree,
        body: ast::CompoundStatementTree,
    ) {
        // FUTURE: check branch/convergence points are consistent before vs after walking body?

        let loop_entry = ir::IrLine::new_jump(
            SourceLoc::none(),
            self.control_flow.next_block(),
            ir::JumpCondition::Unconditional,
        );
        // ignore return value - appending an unconditional jump
        self.append_to_current_block(loop_entry);

        let loop_top = self.current_block;
        let after_loop = self.control_flow.next_block();

        // FUTURE: optimize condition handling to use different jumps
        let condition_result = condition.walk(loc, self);
        let loop_false_condition = ir::JumpCondition::Eq(ir::operands::DualSourceOperands::new(
            condition_result,
            ir::Operand::new_as_unsigned_decimal_constant(0),
        ));

        let loop_false_jump = ir::IrLine::new_jump(loc, after_loop, loop_false_condition);
        let after_loop = self
            .append_to_current_block(loop_false_jump)
            .expect("Expected loop conditonal jump to return target for when condition not met");

        body.walk(self);
        let looping_jump = ir::IrLine::new_jump(loc, loop_top, ir::JumpCondition::Unconditional);
        // ignore return value - appending an unconditional jump
        self.append_to_current_block(looping_jump);

        // done with loop
        self.set_current_block(after_loop);
    }

    pub fn next_temp(&mut self, type_: Type) -> ir::Operand {
        let temp_name = String::from(".T") + &self.temp_num.to_string();
        self.temp_num += 1;
        self.scope()
            .insert_variable(symtab::Variable::new(temp_name.clone(), type_));
        ir::Operand::new_as_temporary(temp_name)
    }

    pub fn push_scope(&mut self, scope: symtab::Scope) {
        self.scopes.push(scope)
    }

    pub fn pop_scope_to_subscope_of_next(&mut self) {
        let popped = self
            .scopes
            .pop()
            .expect("WalkContext::pop_scope_to_subscope_of_next expects valid scope");
        self.scope().insert_subscope(popped);
    }

    pub fn pop_last_scope(&mut self) -> symtab::Scope {
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

    pub fn scope(&mut self) -> &mut symtab::Scope {
        self.scopes
            .last_mut()
            .expect("WalkContext::scope() expects valid scope")
    }

    pub fn lookup_variable_by_name(&self, name: &ir::OperandName) -> Option<&symtab::Variable> {
        for scope in (&self.scopes).into_iter().rev().by_ref() {
            match scope.lookup_variable_by_name(&name.base_name) {
                Some(variable) => return Some(variable),
                None => {}
            }
        }
        None
    }
}

mod tests {
    use crate::{
        frontend::{ast, sourceloc::SourceLoc},
        midend::{ir, linearizer::walkcontext::WalkContext, symtab},
    };

    fn assert_no_branch_or_convergence(context: WalkContext) {
        assert!(context.branch_points.len() == 0);
        assert!(context.convergence_points.len() == 0);
    }

    #[test]
    fn walk_context_initial_state() {
        let context = WalkContext::new();
        assert_no_branch_or_convergence(context);
    }

    #[test]
    fn append_statement() {
        let mut context = WalkContext::new();
        let assignment = ir::IrLine::new_assignment(
            SourceLoc::none(),
            ir::Operand::new_as_variable("dest".into()),
            ir::Operand::new_as_variable("source".into()),
        );
        context.append_statement_to_current_block(assignment);

        assert_no_branch_or_convergence(context);
    }

    #[test]
    fn create_loop() {
        let mut context = WalkContext::new();
        context.push_scope(symtab::Scope::new());

        context.create_loop(
            SourceLoc::none(),
            ast::ExpressionTree {
                loc: SourceLoc::none(),
                expression: { ast::Expression::UnsignedDecimalConstant(123) },
            },
            ast::CompoundStatementTree {
                loc: SourceLoc::none(),
                statements: Vec::new(),
            },
        );

        context.control_flow.to_graphviz();
        assert_no_branch_or_convergence(context);
    }
}
