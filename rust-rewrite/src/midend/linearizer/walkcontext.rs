use crate::{
    frontend::{ast, sourceloc::SourceLoc},
    midend::{ir, symtab, types::Type},
};

use super::treewalk::*;

pub struct WalkContext {
    control_flow: ir::ControlFlow,
    branch_points: Vec<usize>,
    convergence_points: Vec<usize>,
    scopes: Vec<symtab::Scope>,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
    current_block: usize,
    max_block: usize,
}

impl WalkContext {
    pub fn new() -> WalkContext {
        let mut starter_flow = ir::ControlFlow::new();
        WalkContext {
            control_flow: starter_flow,
            branch_points: vec![0],
            convergence_points: vec![1],
            scopes: Vec::new(),
            temp_num: 0,
            current_block: 0,
            max_block: 1,
        }
    }

    pub fn take_control_flow(mut self) -> ir::ControlFlow {
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

    fn replace_branch_and_convergence_points(&mut self, old_block: usize, new_block: usize) {
        for label in &mut self.branch_points {
            if *label == old_block {
                *label = new_block;
            }
        }

        for label in &mut self.convergence_points {
            if *label == old_block {
                *label = new_block;
            }
        }
    }

    // appends the given statement to the current basic block
    // if the statement is any sort of branch, the current block will be updated to be the target of the branch
    // if the branch is conditional, the function returns Some(false_label) where false_label is the target of the
    // block control flows to when the condition is not met
    // for unconditional branches and other statements, returns None
    pub fn append_to_current_block(&mut self, statement: ir::IrLine) -> Option<usize> {
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

    fn create_branching_point(&mut self) {
        self.branch_points.push(self.current_block);
        println!(
            "Create branching point from {}",
            self.branch_points.last().unwrap()
        );
    }

    fn create_convergence_point(&mut self, loc: SourceLoc) {
        let convergence_point = self.next_block(loc);
        self.convergence_points.push(convergence_point);
        println!(
            "Create branching point to {}",
            self.convergence_points.last().unwrap()
        );
    }

    fn set_current_block(&mut self, label: usize) {
        self.current_block = label;
    }

    pub fn create_branching_point_with_convergence(&mut self, loc: SourceLoc) {
        self.create_branching_point();
        self.create_convergence_point(loc);
    }

    pub fn create_branch(&mut self, loc: SourceLoc, condition: ir::JumpCondition) -> Option<usize> {
        assert!(self.branch_points.len() > 0);
        assert!(self.convergence_points.len() > 0);
        assert!(*self.branch_points.last().unwrap() == self.current_block);

        let branch_target = self.next_block(loc);

        println!(
            "Create branch from {}->{}",
            self.current_block, branch_target
        );

        let branch_ir = ir::IrLine::new_jump(loc, branch_target, condition);
        return self.append_to_current_block(branch_ir);
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
            self.current_block, converge_to
        );

        let convergence_jump = ir::IrLine::new_jump(
            SourceLoc::none(),
            converge_to,
            ir::JumpCondition::Unconditional,
        );
        // ignore return value - appending an unconditional jump
        self.append_to_current_block(convergence_jump);
    }

    pub fn finish_branch_and_finalize_convergence(&mut self) {
        let converge_to = self.convergence_points.pop().expect(
            "WalkContext::finish_branch_and_finalize_convergence expects valid convergence point",
        );
        assert!(self.branch_points.pop().is_some());

        let convergence_jump = ir::IrLine::new_jump(
            SourceLoc::none(),
            converge_to,
            ir::JumpCondition::Unconditional,
        );
        // ignore return value - appending an unconditional jump
        self.append_to_current_block(convergence_jump);
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
            self.next_block(loc),
            ir::JumpCondition::Unconditional,
        );
        // ignore return value - appending an unconditional jump
        self.append_to_current_block(loop_entry);

        let loop_top = self.current_block;
        let after_loop = self.next_block(loc);

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

    // finishes the current block, adds a jump to a new block, and sets that new block as the current
    pub fn next_block(&mut self, loc: SourceLoc) -> usize {
        let new_label = self.control_flow.max_block + 1;

        let exit_jump =
            ir::IrLine::new_jump(loc, new_label, ir::operands::JumpCondition::Unconditional);
        // ignore return value - appending an unconditional jump
        self.append_to_current_block(exit_jump);
        new_label
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
