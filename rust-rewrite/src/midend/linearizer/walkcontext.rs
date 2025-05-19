use std::collections::{HashMap, HashSet};

use crate::{
    frontend::{ast, sourceloc::SourceLoc},
    midend::{ir, symtab, symtab::*, types::Type},
};

use super::treewalk::*;

pub struct WalkContext<'a> {
    control_flow: ir::ControlFlow,
    branch_points: HashMap<usize, HashSet<usize>>, // map from branch origin to set of target blocks
    convergence_points: HashMap<usize, usize>, // map of label -> label that block should jump to when done
    global_scope: &'a symtab::Scope,
    scopes: Vec<symtab::Scope>,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
    current_block: usize,
}

impl<'a> WalkContext<'a> {
    pub fn new(global_scope: &'a Scope) -> WalkContext<'a> {
        let starter_flow = ir::ControlFlow::new();

        let mut convergence_points = HashMap::<usize, usize>::new();
        convergence_points.insert(0, 1);

        WalkContext {
            control_flow: starter_flow,
            branch_points: HashMap::<usize, HashSet<usize>>::new(),
            convergence_points,
            global_scope,
            scopes: Vec::new(),
            temp_num: 0,
            current_block: 0,
        }
    }

    pub fn take_control_flow(mut self) -> ir::ControlFlow {
        for (from, to) in self.convergence_points.clone() {
            assert_eq!(self.converge_block(from), to); // TODO: need assert?
        }
        self.control_flow
    }

    fn replace_branch_and_convergence_points(&mut self, old_block: usize, new_block: usize) {
        // replace all instances of old_block with new_block in both branch and convergence point tracking
        // self.branch_points = self
        //     .branch_points
        //     .iter()
        //     .map(|(source, dest_set)| {
        //         (
        //             if *source == old_block {
        //                 new_block
        //             } else {
        //                 *source
        //             },
        //             dest_set
        //                 .into_iter()
        //                 .map(|target| {
        //                     if *target == old_block {
        //                         new_block
        //                     } else {
        //                         *target
        //                     }
        //                 })
        //                 .collect(),
        //         )
        //     })
        //     .collect();

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

    ///appends the given statement to the current basic block
    ///if the statement is any sort of branch, the current block will be updated to be the target of the branch
    ///if the branch is conditional, the function returns Some(false_label) where false_label is the target of the
    ///block control flows to when the condition is not met
    ///for unconditional branches and other statements, returns None
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

    /// creates a branch from the current block based on a condition
    /// returns the target of the branch
    fn create_branch_from_current(&mut self) -> usize {
        let branch_target = self.control_flow.next_block();

        match self.branch_points.get(&self.current_block) {
            Some(_) => panic!(
                "create_branch_from_current called with existing branch (from block {})",
                self.current_block
            ),
            None => {
                self.branch_points
                    .insert(self.current_block, HashSet::new());
            }
        };

        self.branch_points
            .get_mut(&self.current_block)
            .unwrap()
            .insert(branch_target);
        branch_target
    }

    fn add_branch(&mut self, from: usize, to: usize) {
        self.branch_points
            .get_mut(&from)
            .expect("add_branch expects existing branch")
            .insert(to);
    }

    fn add_convergence_point_for_branch(&mut self, from: usize, to: usize) {
        match self.convergence_points.insert(from, to) {
            Some(existing_convergence) => {
                if existing_convergence != to {
                    self.add_convergence_point_for_branch(to, existing_convergence);
                }
            }
            None => {}
        }
    }

    fn create_convergence_points_for_branch(&mut self, branch_from: usize) -> usize {
        let convergence_point = match self.convergence_points.get(&branch_from) {
            Some(existing_convergence) => *existing_convergence,
            None => self.control_flow.next_block(),
        };

        for branch_target in self
            .branch_points
            .get(&branch_from)
            .expect("Creation of convergence points requires existence of branch(es)")
            .clone()
        {
            self.add_convergence_point_for_branch(branch_target, convergence_point);
        }
        convergence_point
    }

    ///append an unconditional jump from the end of block_label to its convergence point
    ///return the block to which control has converged
    fn converge_block(&mut self, block_label: usize) -> usize {
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
        converge_to
    }

    pub fn converge_current_block(&mut self) {
        let converged_to = self.converge_block(self.current_block);
        self.set_current_block(converged_to);
    }

    pub fn set_current_block(&mut self, label: usize) {
        self.current_block = label;
    }

    pub fn create_conditional_branch_from_current(
        &mut self,
        loc: SourceLoc,
        condition: ir::JumpCondition,
    ) -> (usize, Option<usize>) {
        let branch_origin = self.current_block;
        let true_label = self.create_branch_from_current();

        let true_condition_jump = ir::IrLine::new_jump(loc, true_label, condition);
        let maybe_false_label = self.append_to_current_block(true_condition_jump);
        match maybe_false_label {
            Some(false_label) => self.add_branch(branch_origin, false_label),
            None => {}
        }

        self.create_convergence_points_for_branch(branch_origin);

        (true_label, maybe_false_label)
    }

    ///returns the label which control jumps to after the loop
    pub fn create_loop(&mut self, loc: SourceLoc, condition: ast::ExpressionTree) -> usize {
        // first, jump to a fresh block which will be the top of the loop
        let loop_top = self.control_flow.next_block();
        let loop_entry = ir::IrLine::new_jump(loc, loop_top, ir::JumpCondition::Unconditional);
        // ignore return value - appending an unconditional jump
        self.append_to_current_block(loop_entry);

        // FUTURE: optimize condition walk to use different jumps
        // check the condition of the loop, giving us the loop body and loop done labels
        let condition_loc = condition.loc.clone();
        let condition_result: ir::Operand = condition.walk(self).into();
        let loop_condition = ir::JumpCondition::NE(ir::operands::DualSourceOperands::new(
            condition_result,
            ir::Operand::new_as_unsigned_decimal_constant(0),
        ));
        let (_, loop_done_label) =
            self.create_conditional_branch_from_current(condition_loc, loop_condition);
        let loop_done_label = loop_done_label.unwrap();

        self.convergence_points.insert(self.current_block, loop_top);
        self.converge_block(loop_done_label);

        loop_done_label
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

    // TODO: check ordering
    fn all_scopes(&self) -> std::vec::IntoIter<&Scope> {
        let mut scopes_ref: Vec<&Scope> = self.scopes.iter().rev().collect();
        scopes_ref.push(&self.global_scope);
        scopes_ref.into_iter()
    }

    pub fn lookup_variable_by_name(
        &self,
        name: &ir::OperandName,
    ) -> Result<&symtab::Variable, UndefinedSymbolError> {
        for scope in self.all_scopes() {
            match scope.lookup_variable_by_name(&name.base_name) {
                Ok(variable) => return Ok(variable),
                _ => {}
            }
        }

        Err(UndefinedSymbolError::variable(&name.base_name))
    }

    pub fn lookup_type(&self, type_: &Type) -> Result<&TypeDefinition, UndefinedSymbolError> {
        for scope in self.all_scopes() {
            match scope.lookup_type(type_) {
                Ok(variable) => return Ok(variable),
                _ => {}
            }
        }

        Err(UndefinedSymbolError::type_(&type_))
    }

    pub fn lookup_struct(&self, name: &str) -> Result<&StructRepr, UndefinedSymbolError> {
        for scope in self.all_scopes() {
            match scope.lookup_struct(name) {
                Ok(variable) => return Ok(variable),
                _ => {}
            }
        }

        Err(UndefinedSymbolError::struct_(&name))
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        frontend::{ast, sourceloc::SourceLoc},
        midend::{ir, linearizer::walkcontext::WalkContext, symtab},
    };

    fn assert_no_remaining_convergences(context: WalkContext) {
        // allow convergence to block 1 as that should be the final block in the control flow
        for (from, to) in context.convergence_points {
            assert_eq!(to, 1);
        }
    }

    fn assert_branch(context: &WalkContext, from: usize, to: usize) {
        let branches = context.branch_points.get(&from);
        assert!(branches.is_some());
        let branches = branches.unwrap();
        assert!(branches.contains(&to));
    }

    fn assert_convergence(context: &WalkContext, from: usize, to: usize) {
        let convergence = context.convergence_points.get(&from);
        assert_eq!(convergence, Some(&to));
    }

    #[test]
    fn walk_context_initial_state() {
        let global_scope = symtab::Scope::new();
        let context = WalkContext::new(&global_scope);
        assert_no_remaining_convergences(context);
    }

    #[test]
    fn append_statement() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);
        let assignment = ir::IrLine::new_assignment(
            SourceLoc::none(),
            ir::Operand::new_as_variable("dest".into()),
            ir::Operand::new_as_variable("source".into()),
        );
        context.append_statement_to_current_block(assignment);

        assert_no_remaining_convergences(context);
    }

    #[test]
    fn create_branch() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);

        let branch_from = context.current_block;
        let branch_to = context.create_branch_from_current();
        assert_branch(&context, branch_from, branch_to);
        assert_eq!(branch_to, 2);
    }

    #[test]
    fn add_branch() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);

        let branch_from = context.current_block;
        let branch_to = context.create_branch_from_current();

        let second_branch_to = context.control_flow.next_block();
        context.add_branch(branch_from, second_branch_to);

        assert_branch(&context, branch_from, branch_to);
        assert_branch(&context, branch_from, second_branch_to);
        assert_eq!(branch_to, 2);
        assert_eq!(second_branch_to, 3);
    }

    #[test]
    fn simple_branch_convergence_points() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);

        // branch from the current block to somewhere
        let branch_from = context.current_block;
        let branch_to = context.create_branch_from_current();
        assert_branch(&context, branch_from, branch_to);
        assert_eq!(branch_to, 2);

        // create our convergence point, and assert that we converge back to it
        let converge_to = context.create_convergence_points_for_branch(branch_from);
        assert_convergence(&context, branch_to, converge_to);
    }

    #[test]
    fn complex_branch_convergence_points() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);

        // branch from the current block to somewhere
        let branch_from = context.current_block;
        let branch_to = context.create_branch_from_current();
        assert_branch(&context, branch_from, branch_to);

        // also branch to a second place
        let second_branch_to = context.control_flow.next_block();
        context.add_branch(branch_from, second_branch_to);

        // create the convergence point and assert that both branch targets converge to it
        let converge_to = context.create_convergence_points_for_branch(branch_from);
        assert_convergence(&context, branch_to, converge_to);
        assert_convergence(&context, second_branch_to, converge_to);
    }

    #[test]
    fn simple_multiple_branch_convergence_points() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);

        // branch from the current block to somewhere
        let branch_from = context.current_block;
        let branch_to = context.create_branch_from_current();
        assert_branch(&context, branch_from, branch_to);

        // create our convergence point, and assert that we converge back to it
        let converge_to = context.create_convergence_points_for_branch(branch_from);
        assert_convergence(&context, branch_to, converge_to);

        // now, create a second nested branch within the first branch
        context.set_current_block(branch_to);
        let nested_branch_from = context.current_block;
        let nested_branch_to = context.create_branch_from_current();
        assert_branch(&context, nested_branch_from, nested_branch_to);

        // create our convergence point, and assert that we converge back to it
        let nested_converge_to = context.create_convergence_points_for_branch(nested_branch_from);
        assert_convergence(&context, nested_branch_to, nested_converge_to);
    }

    ///test a branch with 2 targets, nested in a branch with one target
    #[test]
    fn complex_multiple_branch_convergence_points() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);

        // branch from the current block to somewhere
        let branch_from = context.current_block;
        let branch_to = context.create_branch_from_current();
        assert_branch(&context, branch_from, branch_to);

        // create our convergence point, and assert that we converge back to it
        let converge_to = context.create_convergence_points_for_branch(branch_from);
        assert_convergence(&context, branch_to, converge_to);

        // now, create a second nested branch within the first branch
        context.set_current_block(branch_to);
        let nested_branch_from = context.current_block;
        let nested_branch_to = context.create_branch_from_current();
        assert_branch(&context, nested_branch_from, nested_branch_to);

        // and add to that second branch a second target
        let second_nested_branch_to = context.control_flow.next_block();
        context.add_branch(nested_branch_from, second_nested_branch_to);
        assert_branch(&context, nested_branch_from, second_nested_branch_to);

        // create our convergence point, and assert that we converge back to it
        let nested_converge_to = context.create_convergence_points_for_branch(nested_branch_from);
        assert_convergence(&context, nested_branch_to, nested_converge_to);
        assert_convergence(&context, nested_branch_to, nested_converge_to);
    }

    ///test a branch with 1 target, nested within a branch with 2 targets
    #[test]
    fn complex_multiple_branch_convergence_points_2() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);

        // branch from the current block to somewhere
        let branch_from = context.current_block;
        let branch_to = context.create_branch_from_current();
        assert_branch(&context, branch_from, branch_to);

        // also branch to a second place
        let second_branch_to = context.control_flow.next_block();
        context.add_branch(branch_from, second_branch_to);
        assert_branch(&context, branch_from, second_branch_to);

        // create our convergence point, and assert that we converge back to it
        let converge_to = context.create_convergence_points_for_branch(branch_from);
        assert_convergence(&context, branch_to, converge_to);
        assert_convergence(&context, second_branch_to, converge_to);

        // now, create a second nested branch within the first branch
        context.set_current_block(branch_to);
        let nested_branch_to = context.create_branch_from_current();
        assert_branch(&context, branch_to, nested_branch_to);

        // create our convergence point, and assert that we converge back to it
        let nested_converge_to = context.create_convergence_points_for_branch(branch_to);
        assert_convergence(&context, branch_to, nested_converge_to);

        // and re-assert our original convergence
        assert_convergence(&context, branch_to, converge_to);
        assert_convergence(&context, second_branch_to, converge_to);
    }

    #[test]
    fn converge_block() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);

        let branch_from = context.current_block;
        let branch_to = context.create_branch_from_current();
        let converge_to = context.create_convergence_points_for_branch(branch_from);

        assert_eq!(context.converge_block(branch_to), converge_to);

        assert_no_remaining_convergences(context);
    }

    #[test]
    fn converge_current_block() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);

        let branch_from = context.current_block;
        let branch_to = context.create_branch_from_current();
        let converge_to = context.create_convergence_points_for_branch(branch_from);

        context.set_current_block(branch_to);
        context.converge_current_block();

        assert_eq!(context.current_block, converge_to);

        assert_no_remaining_convergences(context);
    }

    #[test]
    fn create_loop() {
        let global_scope = symtab::Scope::new();
        let mut context = WalkContext::new(&global_scope);
        context.push_scope(symtab::Scope::new());

        let before_loop = context.current_block;

        let loop_done = context.create_loop(
            SourceLoc::none(),
            ast::ExpressionTree {
                loc: SourceLoc::none(),
                expression: { ast::Expression::UnsignedDecimalConstant(123) },
            },
        );

        assert_ne!(before_loop, context.current_block);
        assert_ne!(loop_done, context.current_block);

        context.converge_current_block();
        context.set_current_block(loop_done);

        assert_no_remaining_convergences(context);
    }
}
