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

    pub fn append_to_current_block(&mut self, statement: ir::IrLine) {
        // TODO: look at moving conditional jump handling entirey to this function?
        let old_current_block = self.current_block;
        // appending the statement, the control flow will tell us if we have a conditional jump
        // it does this by generating an unconditional jump immediately after, and passing back (optionally)
        // a reference to the target field of that jump for us to populate
        let need_new_current_block = self
            .control_flow
            .append_statement_to_block(statement, self.current_block);

        // if control_flow.append_statement_to_block() gave us something
        let set_current_to_max = match need_new_current_block {
            Some(jump_target_new_block) => {
                // bump the new current block, and set the target of the follow-up unconditional jump to that block
                let new_current = self.max_block + 1;
                *jump_target_new_block = new_current;

                // rewrite existing jumps for branch/convergence points to instead point to our new current label
                for label in &mut self.branch_points {
                    if *label == old_current_block {
                        *label = *jump_target_new_block;
                    }
                }

                for label in &mut self.convergence_points {
                    if *label == old_current_block {
                        *label = *jump_target_new_block;
                    }
                }

                // set our current block and actually update the max block
                self.current_block = self.max_block;
                self.max_block += 1;
            }
            None => {}
        };
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

    pub fn create_branch(&mut self, loc: SourceLoc, condition: ir::JumpCondition) {
        assert!(self.branch_points.len() > 0);
        assert!(self.convergence_points.len() > 0);
        assert!(*self.branch_points.last().unwrap() == self.current_block);

        let branch_target = self.next_block(loc);

        println!(
            "Create branch from {}->{}",
            self.current_block, branch_target
        );

        let branch_ir = ir::IrLine::new_jump(loc, branch_target, condition);
        self.append_to_current_block(branch_ir);
        self.set_current_block(branch_target);
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
        self.append_to_current_block(convergence_jump);

        self.set_current_block(
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

        let convergence_jump = ir::IrLine::new_jump(
            SourceLoc::none(),
            converge_to,
            ir::JumpCondition::Unconditional,
        );

        self.append_to_current_block(convergence_jump);

        self.set_current_block(converge_to);
    }

    pub fn create_loop(
        &mut self,
        loc: SourceLoc,
        condition: ast::ExpressionTree,
        body: ast::CompoundStatementTree,
    ) {
        let loop_top = self.next_block(loc);
        let after_loop = self.next_block(loc);
        self.set_current_block(loop_top);

        // FUTURE: optimize condition handling to use different jumps
        let condition_result = condition.walk(loc, self);
        let loop_false_condition = ir::JumpCondition::Eq(ir::operands::DualSourceOperands::from(
            condition_result,
            ir::Operand::new_as_unsigned_decimal_constant(0),
        ));

        let loop_false_jump = ir::IrLine::new_jump(loc, after_loop, loop_false_condition);
        self.append_to_current_block(loop_false_jump);

        body.walk(self);
        let looping_jump = ir::IrLine::new_jump(loc, loop_top, ir::JumpCondition::Unconditional);
        self.append_to_current_block(looping_jump);

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
        self.append_to_current_block(exit_jump);

        self.set_current_block(new_label);
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

    pub fn lookup_variable_by_name(&self, name: &str) -> Option<&symtab::Variable> {
        for scope in (&self.scopes).into_iter().rev().by_ref() {
            match scope.lookup_variable_by_name(name) {
                Some(variable) => return Some(variable),
                None => {}
            }
        }
        None
    }
}
