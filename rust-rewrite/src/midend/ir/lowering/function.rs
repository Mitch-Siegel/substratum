use crate::midend::{ir::*, *};
use std::collections::{HashMap, HashSet};

pub fn find_unlowered_irs(cf: &ControlFlow) -> HashMap<usize, HashSet<usize>> {
    // map from block label to statement indices of unlowered statements
    let mut unlowered = HashMap::<usize, HashSet<usize>>::new();

    for block in cf {
        for (idx, statement) in block.into_iter().enumerate() {
            if !statement.is_lowered() {
                unlowered.entry(block.label).or_default().insert(idx);
            }
        }
    }

    unlowered
}

pub fn lower_function(def_path: symtab::DefPath, symtab: &mut symtab::SymbolTable) {
    let _span = trace::span_auto_debug!("Lower function ", "{}", def_path.last());
    let function = symtab.lookup_at_mut::<symtab::Function>(&def_path).unwrap();

    let (cf, mut unlowered) = match &function.control_flow {
        Some(cf) => {
            let unlowered = find_unlowered_irs(&cf);
            if unlowered.len() == 0 {
                trace::trace!("No unlowered statements in control flow, skipping");
                return;
            }
            (function.control_flow.take().unwrap(), unlowered)
        }
        None => {
            trace::trace!("No control flow, nothing to lower.");
            return;
        }
    };

    trace::trace!("the following statements need lowering: {:?}", unlowered);

    let mut manager = linearizer::BlockManager::from(cf);

    while unlowered.len() > 0 {
        let block = *unlowered.keys().next().unwrap();
        let idx = *unlowered.get(&block).unwrap().iter().next().unwrap();

        let (split_to_block, line_to_deal_with) =
            manager.split_block_at_statement(block, idx).unwrap();

        let after_split_block = manager
            .finish_block_split(split_to_block, line_to_deal_with.loc)
            .unwrap();

        let labels_this_block = unlowered.get_mut(&block).unwrap();
        labels_this_block.remove(&idx);

        // ascertain IRs needing lowering which are still in the same block (before split) and which
        // aren't (after split)
        let before_split: HashSet<usize> = labels_this_block
            .iter()
            .filter(|stmt_idx| *stmt_idx < &idx)
            .cloned()
            .collect();

        let after_split: HashSet<usize> = labels_this_block
            .difference(&before_split)
            .cloned()
            .map(|unaltered_idx| unaltered_idx - idx)
            .collect();

        // remove any after the split
        *labels_this_block = labels_this_block
            .difference(&before_split)
            .cloned()
            .collect();

        // remove the actual record of IRs needing lowering for this block if none are left
        if labels_this_block.len() == 0 {
            unlowered.remove(&block);
        }

        // dump the remaining IR labels from after the split into whatever block we ended up in
        // when everything is done
        for after_split_idx in after_split {
            unlowered
                .entry(after_split_block)
                .or_default()
                .insert(after_split_idx);
        }
    }

    function.control_flow.replace(manager.try_into().unwrap());
}
