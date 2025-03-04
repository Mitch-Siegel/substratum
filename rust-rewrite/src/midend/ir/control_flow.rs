use super::ir;
use serde::Serialize;
use std::{
    collections::{HashMap, HashSet, VecDeque},
    usize,
};

/*
    A ControlFlow represents the notion of ownership over basic blocks. At the end of linearization of a section of code,
    all relevant basic blocks will be owned by a single control flow. During linearization control flows can branch
    correspondingly with actual branches in the code, as they are linarized. Each branch gets its own control flow,
    which owns only the blocks relevant to the contents of branch. When the linearization of each branch is complete,
    its control flow is merged back to the one from which it was branched. The original brancher then takes ownership
    of all blocks of the branchee.
*/

#[derive(Debug, Serialize)]
pub struct ControlFlow {
    pub blocks: Vec<ir::BasicBlock>,
    pub successors: Vec<HashSet<usize>>,
    pub predecessors: Vec<HashSet<usize>>,
    current_block: usize,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
}

impl ControlFlow {
    pub fn new() -> Self {
        ControlFlow {
            blocks: Vec::new(),
            successors: Vec::<HashSet<usize>>::new(),
            predecessors: Vec::<HashSet<usize>>::new(),
            current_block: 0,
            temp_num: 0,
        }
    }

    pub fn labels(&self) -> std::ops::Range<usize> {
        0..self.blocks.len()
    }

    pub fn next_temp(&mut self) -> String {
        let temp_name = String::from(".T") + &self.temp_num.to_string();
        self.temp_num += 1;
        temp_name
    }

    pub fn set_current_block(&mut self, label: usize) {
        assert!(label < self.blocks.len());
        self.current_block = label;
    }

    pub fn current_block(&self) -> usize {
        self.current_block
    }

    pub fn next_block(&mut self) -> usize {
        self.blocks.push(ir::BasicBlock::new());
        self.blocks.len() - 1
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IrLine) {
        self.append_statement_to_block(statement, self.current_block);
    }

    pub fn append_statement_to_block(&mut self, statement: ir::IrLine, block: usize) {
        match &statement.operation {
            ir::Operations::Jump(operands) => {
                let target_block = operands.destination_block;
                while self.predecessors.len() <= target_block {
                    self.predecessors.push(HashSet::<usize>::new());
                }

                while self.successors.len() <= target_block {
                    self.successors.push(HashSet::<usize>::new());
                }

                self.predecessors[target_block].insert(self.current_block);
                self.successors[self.current_block].insert(target_block);
            }
            _ => {}
        }

        self.blocks[block].push(statement);
    }

    pub fn assign_program_points(&mut self) {
        unimplemented!();
    }

    pub fn to_graphviz(&self, depths: &HashMap<usize, usize>) {
        print!("digraph {{fontname=\"consolas\"; node[shape=box; fontname=\"consolas\"; nojustify=true]; splines=ortho;");
        for (label, _depth) in depths {
            todo!("reimplement with new cfgblocks data structure");
            // print!("{}[label=\"{}\\l\"]; ", label, self.block_to_string(*label));
        }

        for label in 0..self.blocks.len() {
            for successor in self.successors.get(label).unwrap() {
                print!("{}->{}; ", label, successor)
            }
        }

        let mut depth_vec = Vec::<HashSet<usize>>::new();
        for (block, depth) in depths {
            while depth_vec.len() <= *depth {
                depth_vec.push(HashSet::<usize>::new());
            }

            depth_vec[*depth].insert(*block);
        }

        for rank in depth_vec {
            print!("{{rank=same; ");
            for block in rank {
                print!("{};", block);
            }
            println!("}}");
        }

        println!("}}");
    }
}

struct ControlFlowBfs<'a> {
    pub control_flow: &'a mut ControlFlow,
    pub visited: HashSet<usize>,
    pub queue: VecDeque<usize>,
    pub next_queue: VecDeque<usize>,
}

impl<'a> ControlFlowBfs<'a> {
    pub fn map<MetadataType>(
        control_flow: &'a mut ControlFlow,
        operation: fn(&mut ir::BasicBlock, &mut MetadataType),
        metadata: &mut MetadataType,
    ) where
        MetadataType: std::fmt::Display,
    {
        let mut bfs = Self::new(control_flow);
        bfs.visit_all(operation, metadata);
    }

    fn new(control_flow: &'a mut ControlFlow) -> Self {
        Self {
            control_flow: control_flow,
            visited: HashSet::<usize>::new(),
            queue: VecDeque::<usize>::new(),
            next_queue: VecDeque::<usize>::new(),
        }
    }

    fn visit_all<MetadataType>(
        &mut self,
        on_visit: fn(&mut ir::BasicBlock, &mut MetadataType),
        metadata: &mut MetadataType,
    ) {
        self.queue.push_back(0);

        // go until done
        while (self.queue.len() > 0) || (self.next_queue.len() > 0) {
            match &self.queue.pop_front() {
                Some(label) => {
                    // only visit once
                    if !self.is_visited(label) {
                        // ensure all the predecessors visited before visiting
                        if self.visited_all_predecessors(*label) {
                            on_visit(&mut self.control_flow.blocks[*label], metadata);

                            self.add_successors_to_next(label);
                            self.mark_visited(label);
                        } else {
                            // if we haven't visited all predecessors, can't visit the block now
                            self.next_queue.push_back(*label);
                        }
                    }
                }
                None => {
                    // all done at this depth, swap the 'next' queue to be our current
                    self.swap_to_next_queue();
                }
            }
        }
    }

    fn is_visited(&self, block_label: &usize) -> bool {
        self.visited.contains(block_label)
    }

    fn visited_all_predecessors(&self, block_label: usize) -> bool {
        let mut visited_all_predecessors = true;
        for p in &self.control_flow.predecessors[block_label] {
            if !self.is_visited(p) {
                visited_all_predecessors = false;
                break;
            }
        }

        visited_all_predecessors
    }

    fn mark_visited(&mut self, block_label: &usize) {
        self.visited.insert(*block_label);
    }

    fn add_successors_to_next(&mut self, block_label: &usize) {
        for successor in &self.control_flow.successors[*block_label] {
            self.next_queue.push_back(*successor);
        }
    }

    fn swap_to_next_queue(&mut self) {
        assert!(self.queue.len() == 0); // verify current queue actually empty

        // copy and clear the 'next' queue
        let potential_next = self.next_queue.clone();
        self.next_queue.clear();

        // it is possible that a node in the 'next' queue is still unvisitable
        // that is - it was bumped to the 'next' queue, the current queue was finished, and all predecessors of it are still not visited
        // in this case, we need to re-bump that node to the 'next' queue when swapping queues
        for potential_next in potential_next {
            if self.visited_all_predecessors(potential_next) {
                self.queue.push_back(potential_next);
            } else {
                self.next_queue.push_back(potential_next);
            }
        }
    }
}

impl ControlFlow {
    pub fn map_over_blocks_mut_by_bfs<MetadataType>(
        &mut self,
        operation: fn(&mut ir::BasicBlock, &mut MetadataType),
        metadata: &mut MetadataType,
    ) where
        MetadataType: std::fmt::Display,
    {
        ControlFlowBfs::map(self, operation, metadata)
    }
}
