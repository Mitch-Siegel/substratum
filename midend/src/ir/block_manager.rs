use frontend::loc;

use crate::{
    ir::{BasicBlock, ControlFlow, ValueInterner, block_manager::branch::Epilogue},
    symtab::{self, ValueOwner},
    types,
};

use std::{collections::HashMap, fmt::Debug};

pub(crate) mod branch;

use branch::{Branch, UnfinishedFunction};

#[derive(Debug, Clone)]
pub(crate) struct BlockManager<B: Branch> {
    max_block: usize,
    // branch path of basic block labels targeted by the branches which got us to current_block
    blocks: HashMap<usize, BasicBlock>,
    values: ValueInterner<Option<types::Syntactic>>,
    branch_state: B,
}

impl<B: Branch> BlockManager<B> {
    // returns (Self, start_block)
    // where start_block is the first basic block in the function
    pub(crate) fn new(
        parent_def_path: &symtab::ValuePath,
        start_loc: loc::Loc,
        end_loc: loc::Loc,
    ) -> BlockManager<UnfinishedFunction> {
        // set up the initlal convergence - must always end up at the end_block
        let start_block_path = parent_def_path.clone().with_scope(symtab::ScopeId(0));
        let end_block_path = parent_def_path.clone().with_scope(symtab::ScopeId(1));
        let start_block = BasicBlock::new(0, start_block_path, start_loc);
        let end_block = BasicBlock::new(1, end_block_path, end_loc);

        let start_label = start_block.label;

        BlockManager {
            max_block: 1,
            blocks: vec![(start_block.label, start_block)].into_iter().collect(),
            values: ValueInterner::new(Some(types::Syntactic::Unit), frontend::here!()),
            branch_state: UnfinishedFunction {
                cur_block: start_label,
                epilogue: end_block,
            },
        }
    }

    pub(crate) fn erase(self) -> BlockManager<Box<dyn Branch>> {
        BlockManager {
            max_block: self.max_block,
            blocks: self.blocks,
            values: self.values,
            branch_state: Box::new(self.branch_state) as Box<dyn Branch>,
        }
    }

    pub(crate) fn current_block_mut(&mut self) -> &mut BasicBlock {
        let label = self.branch_state.cur_block();
        self.blocks.get_mut(&label).unwrap()
    }

    pub(crate) fn current_label(&self) -> usize {
        self.branch_state.cur_block()
    }

    pub(crate) fn with_existing_blocks(
        blocks: HashMap<usize, BasicBlock>,
        block_to_be_current: usize,
        existing_values: ValueInterner<Option<types::Syntactic>>,
    ) -> BlockManager<branch::Epilogue> {
        let mut max_block = 0;
        for label in blocks.keys() {
            max_block = max_block.max(*label);
        }

        BlockManager {
            max_block,
            branch_state: branch::Epilogue {
                cur_block: block_to_be_current,
            },
            blocks,
            values: existing_values,
        }
    }

    pub(crate) fn values(&self) -> &ValueInterner<Option<types::Syntactic>> {
        &self.values
    }

    pub(crate) fn values_mut(&mut self) -> &mut ValueInterner<Option<types::Syntactic>> {
        &mut self.values
    }

    pub(crate) fn branch_state(&self) -> &B {
        &self.branch_state
    }
}

impl BlockManager<Box<dyn Branch>> {
    pub(crate) fn downcast<Target: Branch>(self) -> BlockManager<Target> {
        BlockManager {
            max_block: self.max_block,
            blocks: self.blocks,
            values: self.values,
            branch_state: *self
                .branch_state
                .as_any_box()
                .downcast::<Target>()
                .expect("bad branch type"),
        }
    }
}

impl From<(ControlFlow, usize)> for BlockManager<Epilogue> {
    fn from(value: (ControlFlow, usize)) -> Self {
        let (cf, block_to_be_current) = value;
        let (blocks, values) = cf.take();
        Self::with_existing_blocks(blocks, block_to_be_current, values)
    }
}

#[cfg(test)]
mod tests {}
