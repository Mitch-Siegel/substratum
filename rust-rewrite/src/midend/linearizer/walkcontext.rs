use std::collections::{HashMap, HashSet};

use crate::{
    frontend::{ast, sourceloc::SourceLoc},
    midend::{ir, linearizer::*, symtab, types::Type},
};

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
