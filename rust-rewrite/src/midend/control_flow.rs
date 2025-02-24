use super::{
    basic_block::BasicBlock,
    ir::{self, *},
};
use serde::Serialize;
use std::{
    collections::{HashMap, HashSet, VecDeque},
    usize,
};

/*
    A ControlFlow represents the notion of ownership over BasicBlocks. At the end of linearization of a section of code,
    all relevant basic blocks will be owned by a single control flow. During linearization control flows can branch
    correspondingly with actual branches in the code, as they are linarized. Each branch gets its own control flow,
    which owns only the blocks relevant to the contents of branch. When the linearization of each branch is complete,
    its control flow is merged back to the one from which it was branched. The original brancher then takes ownership
    of all blocks of the branchee.
*/

#[derive(Debug, Serialize)]
pub struct ControlFlow {
    pub blocks: Vec<BasicBlock>,
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

    pub fn next_block(&mut self) -> &mut BasicBlock {
        self.blocks.push(BasicBlock::new(self.blocks.len()));
        self.blocks.last_mut().unwrap()
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IR) {
        match &statement.operation {
            ir::BasicOperations::Jump(operands) => {
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

        self.blocks[self.current_block].append_statement(statement);
    }

    pub fn append_statement_to_block(&mut self, statement: IR, block: usize) {
        self.blocks[block].append_statement(statement);
    }

    pub fn print_ir(&self) {
        for block in &self.blocks {
            println!("Block {}:", block.label());
            println!("{}", block);
        }
    }

    pub fn assign_program_points(&mut self) {
        let depths = BlockDepths::find(self);

        for block in &mut self.blocks {
            block.assign_depth(depths[&block.label()])
        }
    }

    pub fn to_graphviz(&self, depths: &HashMap<usize, usize>) {
        print!("digraph {{fontname=\"consolas\"; node[shape=box; fontname=\"consolas\"; nojustify=true]; splines=ortho;");
        for (block, depth) in depths {
            println!("{}[label=\"{}\\l\"]; ", block, self.blocks[*block]);
        }

        for block in &self.blocks {
            for successor in self.successors.get(block.label()).unwrap() {
                print!("{}->{}; ", block.label(), successor)
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

struct BlockDepths<'a> {
    pub control_flow: &'a ControlFlow,
    pub visited: HashSet<usize>,
    pub depths: HashMap<usize, usize>,
    pub queue: VecDeque<usize>,
    pub next_queue: VecDeque<usize>,
}

impl<'a> BlockDepths<'a> {
    fn new(control_flow: &'a ControlFlow) -> Self {
        Self {
            control_flow,
            visited: HashSet::<usize>::new(),
            depths: HashMap::<usize, usize>::new(),
            queue: VecDeque::<usize>::new(),
            next_queue: VecDeque::<usize>::new(),
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

    fn bfs_for_depths(&mut self) {
        let mut current_depth: usize = 0;
        self.queue.push_back(0);

        // go until done
        while (self.queue.len() > 0) || (self.next_queue.len() > 0) {
            match &self.queue.pop_front() {
                Some(label) => {
                    // only visit once
                    if !self.is_visited(label) {
                        // ensure all the predecessors visited before visiting
                        if self.visited_all_predecessors(*label) {
                            // assign depth to this block, add its successors to the 'next' queue, and mark as visited
                            self.depths.insert(*label, current_depth);
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
                    current_depth += 1;
                    self.swap_to_next_queue();
                }
            }
        }
    }

    pub fn find(control_flow: &ControlFlow) -> HashMap<usize, usize> {
        let mut depths = BlockDepths::new(control_flow);
        depths.bfs_for_depths();
        depths.depths
    }
}
