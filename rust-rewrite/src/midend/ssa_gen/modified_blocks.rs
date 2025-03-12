use crate::midend::ir;
use std::collections::HashMap;

#[derive(Debug)]
pub struct ModifiedBlocks {
    blocks: HashMap<usize, ir::BasicBlock>,
}

impl ModifiedBlocks {
    pub fn new() -> Self {
        ModifiedBlocks {
            blocks: HashMap::new(),
        }
    }

    pub fn add_block(&mut self, block: ir::BasicBlock) {
        let label = block.label();
        if self.blocks.insert(label, block).is_some() {
            panic!(
                "Block {} already modified in this pass of SSA read conversion",
                label
            );
        }
    }

    pub fn take(self) -> HashMap<usize, ir::BasicBlock> {
        self.blocks
    }
}
