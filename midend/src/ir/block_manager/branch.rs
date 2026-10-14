use std::fmt;

use frontend::loc;

use crate::{
    ir::{self, BasicBlock, ControlFlow, block_manager::BlockManager, lowered::JumpCondition},
    symtab,
};

pub(crate) trait Branch: 'static {
    fn cur_block(&self) -> usize;

    fn set_cur_block(&mut self, block: usize);

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any>;
}

impl Branch for Box<dyn Branch> {
    fn cur_block(&self) -> usize {
        (**self).cur_block()
    }

    fn set_cur_block(&mut self, block: usize) {
        (**self).set_cur_block(block);
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        (*self).as_any_box()
    }
}

pub(crate) struct NestedBranch<P: Branch, B: Branch> {
    pub(crate) parent: P,
    pub(crate) branch: B,
}

impl<P: Branch, B: Branch> Branch for NestedBranch<P, B> {
    fn cur_block(&self) -> usize {
        self.branch.cur_block()
    }
    fn set_cur_block(&mut self, block: usize) {
        self.branch.set_cur_block(block);
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        self
    }
}

impl<P: Branch, B: Branch> fmt::Debug for NestedBranch<P, B> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "({}, {})",
            std::any::type_name::<P>(),
            std::any::type_name::<B>(),
        )
    }
}

#[derive(Debug)]
pub(crate) struct Unconditional {
    pub(super) cur_block: usize,
    pub(super) dest: BasicBlock,
}

impl Branch for Unconditional {
    fn cur_block(&self) -> usize {
        self.cur_block
    }
    fn set_cur_block(&mut self, block: usize) {
        self.cur_block = block;
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        self
    }
}

#[derive(Debug)]
pub(crate) struct DoubleBranchTrue {
    cur_block: usize,
    false_: BasicBlock,
    after_branch: BasicBlock,
}
impl Branch for DoubleBranchTrue {
    fn cur_block(&self) -> usize {
        self.cur_block
    }
    fn set_cur_block(&mut self, block: usize) {
        self.cur_block = block;
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        self
    }
}

pub(crate) struct DoubleBranchFalse {
    cur_block: usize,
    after_branch: BasicBlock,
}
impl Branch for DoubleBranchFalse {
    fn cur_block(&self) -> usize {
        self.cur_block
    }
    fn set_cur_block(&mut self, block: usize) {
        self.cur_block = block;
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        self
    }
}

#[derive(Debug)]
pub(crate) struct SingleBranchTrue {
    cur_block: usize,
    after_branch: BasicBlock,
}
impl Branch for SingleBranchTrue {
    fn cur_block(&self) -> usize {
        self.cur_block
    }
    fn set_cur_block(&mut self, block: usize) {
        self.cur_block = block;
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        self
    }
}

pub(crate) struct LoopHeader {
    cur_block: usize,
    loop_body: BasicBlock,
    loop_ftr: BasicBlock,
    after_loop: BasicBlock,
}

impl LoopHeader {
    pub(crate) fn after_loop_label(&self) -> usize {
        self.after_loop.label
    }
}

impl Branch for LoopHeader {
    fn cur_block(&self) -> usize {
        self.cur_block
    }
    fn set_cur_block(&mut self, block: usize) {
        self.cur_block = block;
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        self
    }
}

pub(crate) struct LoopBody {
    cur_block: usize,
    loop_ftr: BasicBlock,
    after_loop: BasicBlock,
}

impl LoopBody {
    // FUTURE: breaks will target this label
    #[allow(unused)]
    pub(crate) fn after_loop_label(&self) -> usize {
        self.after_loop.label
    }
}

impl Branch for LoopBody {
    fn cur_block(&self) -> usize {
        self.cur_block
    }
    fn set_cur_block(&mut self, block: usize) {
        self.cur_block = block;
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        self
    }
}

pub(crate) struct LoopFooter {
    cur_block: usize,
    after_loop: BasicBlock,
}
impl Branch for LoopFooter {
    fn cur_block(&self) -> usize {
        self.cur_block
    }
    fn set_cur_block(&mut self, block: usize) {
        self.cur_block = block;
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        self
    }
}

pub(crate) struct UnfinishedFunction {
    pub(super) cur_block: usize,
    pub(super) epilogue: BasicBlock,
}

impl Branch for UnfinishedFunction {
    fn cur_block(&self) -> usize {
        self.cur_block
    }
    fn set_cur_block(&mut self, block: usize) {
        self.cur_block = block;
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        self
    }
}

pub(crate) struct Epilogue {
    pub(super) cur_block: usize,
}
impl Branch for Epilogue {
    fn cur_block(&self) -> usize {
        self.cur_block
    }
    fn set_cur_block(&mut self, block: usize) {
        self.cur_block = block;
    }

    fn as_any_box(self: Box<Self>) -> Box<dyn std::any::Any> {
        self
    }
}

/// describes the source code location and scope defpath of the target of a branch
pub(crate) struct BranchTarget {
    pub loc: loc::Loc,
    pub path: symtab::ScopePath,
}

impl BranchTarget {
    pub(crate) fn new(loc: loc::Loc, path: symtab::ScopePath) -> Self {
        Self { loc, path }
    }
}

pub(crate) struct BranchDesc {
    /// the location of the branch itself
    pub loc: loc::Loc,
    /// the first source location after anything relevant to the branch
    pub after: loc::Loc,
}

impl BranchDesc {
    pub(crate) fn new(loc: loc::Loc, after: loc::Loc) -> Self {
        Self { loc, after }
    }
}

/// Conditional and unconditional branches
impl<S: Branch> BlockManager<S> {
    fn with_nested_branch<B: Branch>(self, branch: B) -> BlockManager<NestedBranch<S, B>> {
        BlockManager {
            max_block: self.max_block,
            blocks: self.blocks,
            values: self.values,
            branch_state: NestedBranch {
                parent: self.branch_state,
                branch,
            },
        }
    }

    // returns (branch_target, after_branch)
    // TODO: this isn't a branch it should just actually split us off to a new block
    pub(crate) fn create_unconditional_branch(
        mut self,
        desc: BranchDesc,
        parent_def_path: symtab::ScopePath,
        true_def_path: symtab::ScopePath,
    ) -> BlockManager<NestedBranch<S, Unconditional>> {
        self.max_block += 2;
        let true_block = ir::BasicBlock::new(self.max_block - 1, true_def_path, desc.loc.clone());
        let after_branch = ir::BasicBlock::new(self.max_block, parent_def_path, desc.after);
        let after_branch_label = after_branch.label;

        let from = self.current_label();

        trace::trace!(
            "unconditional branch from block {} - true block: {}, after branch: {}",
            from,
            true_block.label,
            after_branch_label
        );

        let unconditional_jump = ir::IrLine::new_jump(
            desc.loc,
            true_block.label,
            ir::lowered::operands::JumpCondition::Unconditional,
        );
        self.current_block_mut().push(unconditional_jump);

        let true_block_label = true_block.label;
        self.blocks.insert(true_block_label, true_block);
        self.with_nested_branch(Unconditional {
            cur_block: true_block_label,
            dest: after_branch,
        })
    }

    // returns the label of the true branch or BranchError
    pub(crate) fn create_double_branch(
        mut self,
        desc: BranchDesc,
        jump_condition: ir::lowered::operands::JumpCondition,
        true_data: BranchTarget,
        false_data: BranchTarget, // TODO: make this an option to support true-only branch conditioning
        parent_def_path: symtab::ScopePath,
    ) -> BlockManager<NestedBranch<S, DoubleBranchTrue>> {
        self.max_block += 3;

        let BranchTarget {
            loc: true_loc,
            path: true_path,
        } = true_data;
        let BranchTarget {
            loc: false_loc,
            path: false_path,
        } = false_data;

        let true_block = ir::BasicBlock::new(self.max_block - 2, true_path, true_loc);
        let false_block = ir::BasicBlock::new(self.max_block - 1, false_path, false_loc);
        let after_branch_block = ir::BasicBlock::new(self.max_block, parent_def_path, desc.after);

        let from = self.current_label();

        trace::trace!(
            "create conditional branch from block {} - true block: {}, false block: {}, after branch: {}",
            from,
            true_block.label,
            false_block.label,
            after_branch_block.label
        );

        let from_block = self.current_block_mut();

        let conditional_jump =
            ir::IrLine::new_jump(desc.loc.clone(), true_block.label, jump_condition);
        from_block.push(conditional_jump);
        let unconditional_jump = ir::IrLine::new_jump(
            desc.loc,
            false_block.label,
            ir::lowered::operands::JumpCondition::Unconditional,
        );
        from_block.push(unconditional_jump);

        let true_label = true_block.label;
        self.blocks.insert(true_label, true_block);
        self.with_nested_branch(DoubleBranchTrue {
            cur_block: true_label,
            false_: false_block,
            after_branch: after_branch_block,
        })
    }

    pub(crate) fn create_single_branch(
        mut self,
        desc: BranchDesc,
        jump_condition: ir::lowered::operands::JumpCondition,
        true_data: BranchTarget,
        parent_def_path: symtab::ScopePath,
    ) -> BlockManager<NestedBranch<S, SingleBranchTrue>> {
        self.max_block += 2;
        let BranchTarget {
            loc: true_loc,
            path: true_path,
        } = true_data;

        let true_block = ir::BasicBlock::new(self.max_block - 1, true_path, true_loc);
        let after_block = ir::BasicBlock::new(self.max_block, parent_def_path, desc.after);

        let from = self.current_label();

        trace::trace!(
            "create conditional single branch from block {} - true block: {}, after branch: {}",
            from,
            true_block.label,
            after_block.label
        );

        let from_block = self.current_block_mut();

        let conditional_jump =
            ir::IrLine::new_jump(desc.loc.clone(), true_block.label, jump_condition);
        from_block.push(conditional_jump);
        let unconditional_jump = ir::IrLine::new_jump(
            desc.loc,
            after_block.label,
            ir::lowered::operands::JumpCondition::Unconditional,
        );
        from_block.push(unconditional_jump);

        let true_label = true_block.label;
        self.blocks.insert(true_label, true_block);
        self.with_nested_branch(SingleBranchTrue {
            cur_block: true_label,
            after_branch: after_block,
        })
    }
}

impl<P: Branch, C: Branch> BlockManager<NestedBranch<P, C>> {
    fn without_nested_branch(self) -> (BlockManager<P>, C) {
        let NestedBranch { parent, branch } = self.branch_state;

        (
            BlockManager {
                max_block: self.max_block,
                blocks: self.blocks,
                values: self.values,
                branch_state: parent,
            },
            branch,
        )
    }
}

impl<P: Branch> BlockManager<NestedBranch<P, DoubleBranchTrue>> {
    // returns the label of the false branch or BranchError
    pub(crate) fn finish_true_branch_switch_to_false(
        mut self,
        loc: loc::Loc,
    ) -> BlockManager<NestedBranch<P, DoubleBranchFalse>> {
        let exit_jmp = ir::IrLine::new_jump(
            loc,
            self.branch_state.branch.after_branch.label,
            JumpCondition::Unconditional,
        );
        self.current_block_mut().push(exit_jmp);

        let (mut ret, condition): (BlockManager<P>, DoubleBranchTrue) =
            self.without_nested_branch();

        let DoubleBranchTrue {
            cur_block: _,
            false_,
            after_branch,
        } = condition;

        let false_label = false_.label;
        ret.blocks.insert(false_label, false_);

        ret.with_nested_branch(DoubleBranchFalse {
            cur_block: false_label,
            after_branch,
        })
    }
}

impl<P: Branch> BlockManager<NestedBranch<P, DoubleBranchFalse>> {
    pub(crate) fn finish_branch(mut self, loc: loc::Loc) -> BlockManager<P> {
        let exit_jmp = ir::IrLine::new_jump(
            loc,
            self.branch_state.branch.after_branch.label,
            JumpCondition::Unconditional,
        );
        self.current_block_mut().push(exit_jmp);

        let (mut ret, false_) = self.without_nested_branch();

        #[allow(unused)]
        let DoubleBranchFalse {
            cur_block,
            after_branch,
        } = false_;

        let after_label = after_branch.label;
        ret.blocks.insert(after_label, after_branch);
        ret.branch_state.set_cur_block(after_label);

        ret
    }
}

impl<P: Branch> BlockManager<NestedBranch<P, SingleBranchTrue>> {
    pub(crate) fn finish_branch(mut self, loc: loc::Loc) -> BlockManager<P> {
        let exit_jmp = ir::IrLine::new_jump(
            loc,
            self.branch_state.branch.after_branch.label,
            JumpCondition::Unconditional,
        );
        self.current_block_mut().push(exit_jmp);
        let (mut ret, false_) = self.without_nested_branch();

        let SingleBranchTrue {
            cur_block: _,
            after_branch,
        } = false_;

        let after_label = after_branch.label;
        ret.blocks.insert(after_label, after_branch);
        ret.branch_state.set_cur_block(after_label);

        ret
    }
}

impl<P: Branch> BlockManager<NestedBranch<P, Unconditional>> {
    pub(crate) fn finish_unconditional_branch(mut self, loc: loc::Loc) -> BlockManager<P> {
        let exit_jmp = ir::IrLine::new_jump(
            loc,
            self.branch_state.branch.dest.label,
            JumpCondition::Unconditional,
        );
        self.current_block_mut().push(exit_jmp);
        let (mut ret, unconditional) = self.without_nested_branch();

        let Unconditional { dest, .. } = unconditional;

        let dest_label = dest.label;
        ret.blocks.insert(dest_label, dest);
        ret.branch_state.set_cur_block(dest_label);

        ret
    }
}

/// Loops
impl<B: Branch> BlockManager<B> {
    // returns (loop_top, loop_bottom, after_loop)
    pub(crate) fn create_loop(
        mut self,
        desc: BranchDesc,
        hdr_loc: loc::Loc,
        body_loc: loc::Loc,
        ftr_loc: loc::Loc,
        parent_def_path: symtab::ScopePath,
        loop_def_path: symtab::ScopePath,
    ) -> BlockManager<NestedBranch<B, LoopHeader>> {
        self.max_block += 4;
        let loop_hdr = ir::BasicBlock::new(self.max_block - 3, loop_def_path.clone(), hdr_loc);
        let loop_body = ir::BasicBlock::new(self.max_block - 2, loop_def_path.clone(), body_loc);
        let loop_ftr = ir::BasicBlock::new(self.max_block - 1, loop_def_path, ftr_loc);
        let after_loop = ir::BasicBlock::new(self.max_block, parent_def_path, desc.after);

        let from = self.current_label();

        trace::trace!(
            "create loop from block {} - loop top: {}, loop bottom: {}, after loop: {}",
            from,
            loop_hdr.label,
            loop_ftr.label,
            after_loop.label,
        );

        // transfer control flow unconditionally to the top of the loop
        let enter_header = ir::IrLine::new_jump(
            desc.loc,
            loop_hdr.label,
            ir::lowered::operands::JumpCondition::Unconditional,
        );
        self.current_block_mut().push(enter_header);

        // now the current block should be the loop's top
        let loop_hdr_label = loop_hdr.label;
        self.blocks.insert(loop_hdr_label, loop_hdr);
        self.with_nested_branch(LoopHeader {
            cur_block: loop_hdr_label,
            loop_body,
            loop_ftr,
            after_loop,
        })
    }
}

impl<P: Branch> BlockManager<NestedBranch<P, LoopHeader>> {
    pub(crate) fn finish_loop_header(
        self,
        _loc: loc::Loc,
    ) -> BlockManager<NestedBranch<P, LoopBody>> {
        let (mut ret, hdr) = self.without_nested_branch();

        let LoopHeader {
            cur_block: _,
            loop_body,
            loop_ftr,
            after_loop,
        } = hdr;

        let body_label = loop_body.label;
        ret.blocks.insert(body_label, loop_body);

        ret.with_nested_branch(LoopBody {
            cur_block: body_label,
            loop_ftr,
            after_loop,
        })
    }
}

impl<P: Branch> BlockManager<NestedBranch<P, LoopBody>> {
    pub(crate) fn finish_loop_body(
        self,
        _loc: loc::Loc,
    ) -> BlockManager<NestedBranch<P, LoopFooter>> {
        let (mut ret, body) = self.without_nested_branch();

        let LoopBody {
            cur_block: _,
            loop_ftr,
            after_loop,
        } = body;

        let ftr_label = loop_ftr.label;
        ret.blocks.insert(ftr_label, loop_ftr);

        ret.with_nested_branch(LoopFooter {
            cur_block: ftr_label,
            after_loop,
        })
    }
}

impl<P: Branch> BlockManager<NestedBranch<P, LoopFooter>> {
    pub(crate) fn finish_loop_footer(self, _loc: loc::Loc) -> BlockManager<P> {
        let (mut ret, ftr) = self.without_nested_branch();

        let LoopFooter {
            cur_block: _,
            after_loop,
        } = ftr;

        let after_label = after_loop.label;
        ret.blocks.insert(after_label, after_loop);

        ret.branch_state.set_cur_block(after_label);
        ret
    }
}

impl BlockManager<UnfinishedFunction> {
    pub(crate) fn finish_to_epilogue(mut self, loc: loc::Loc) -> BlockManager<Epilogue> {
        let epilogue_label = self.branch_state.epilogue.label;

        let to_epilogue_block = ir::IrLine::new_jump(
            loc,
            epilogue_label,
            ir::lowered::JumpCondition::Unconditional,
        );
        self.current_block_mut().push(to_epilogue_block);

        let UnfinishedFunction {
            cur_block: _,
            epilogue,
        } = self.branch_state;

        self.blocks.insert(epilogue_label, epilogue);

        BlockManager {
            max_block: self.max_block,
            blocks: self.blocks,
            values: self.values,
            branch_state: Epilogue {
                cur_block: epilogue_label,
            },
        }
    }
}

impl From<BlockManager<Epilogue>> for ControlFlow {
    fn from(value: BlockManager<Epilogue>) -> Self {
        Self::new(value.blocks, value.values)
    }
}
