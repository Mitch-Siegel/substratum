// use std::collections::{BTreeSet, HashMap, HashSet};

// use crate::midend::{
//     ir::{self, ControlFlow},
//     symtab::Function,
// };

// pub fn remove_unused_blocks(function: &mut Function) {
//     let mut block_references = HashMap::<usize, usize>::new();

//     for label_num in 0..function.control_flow.blocks.len() {
//         block_references.entry(label_num).or_insert(0);
//         for target in &function.control_flow.successors[label_num] {
//             *block_references.entry(*target).or_insert(0) += 1;
//         }
//     }

//     let mut block_labels_to_remove = BTreeSet::<usize>::new();

//     for (block, rc) in block_references {
//         println!("{}:{}", block, rc);
//         if rc == 0 {
//             block_labels_to_remove.insert(block);
//         }
//     }

//     let mut block_labels_to_rename = HashMap::<usize, usize>::new();
//     for label_num in &block_labels_to_remove {
//         for affected in *label_num..function.control_flow.blocks.len() {
//             if !block_labels_to_remove.contains(&affected) {
//                 *block_labels_to_rename.entry(affected).or_insert(affected) -= 1;
//             }
//         }
//     }

//     for (orig, renamed) in &block_labels_to_rename {
//         println!("{}->{}", orig, renamed);
//     }

//     for block in &mut function.control_flow.blocks {
//         for statement in block.statements_mut() {
//             match &mut statement.operation {
//                 ir::Operations::Jump(jump) => {
//                     let original_destination = jump.destination_block;
//                     jump.destination_block = match block_labels_to_rename.get(&original_destination)
//                     {
//                         Some(new_label) => *new_label,
//                         None => original_destination,
//                     };
//                 }
//                 _ => {}
//             }
//         }
//     }

//     for (old_label, new_label) in &block_labels_to_rename {
//         function.control_flow.blocks[*old_label].label = *new_label;
//         println!("Rename {}->{}", old_label, new_label);
//     }

//     for remove in block_labels_to_remove.iter().rev() {
//         println!("Remove unused block {}", remove);
//         function.control_flow.blocks.remove(*remove);
//         function.control_flow.successors.remove(*remove);
//         function.control_flow.predecessors.remove(*remove);
//     }

//     for predecessor_set in &mut function.control_flow.predecessors {
//         let mut new_set = HashSet::<usize>::new();

//         for predecessor in predecessor_set.iter() {
//             let original_predecessor = predecessor;
//             new_set.insert(match block_labels_to_rename.get(&original_predecessor) {
//                 Some(new_label) => *new_label,
//                 None => *predecessor,
//             });
//         }

//         *predecessor_set = new_set;
//     }

//     for successor_set in &mut function.control_flow.successors {
//         let mut new_set = HashSet::<usize>::new();

//         for successor in successor_set.iter() {
//             let original_predecessor = successor;
//             new_set.insert(match block_labels_to_rename.get(&original_predecessor) {
//                 Some(new_label) => *new_label,
//                 None => *successor,
//             });
//         }

//         *successor_set = new_set;
//     }

//     function.control_flow.to_graphviz();
// }
