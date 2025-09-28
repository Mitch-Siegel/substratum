use crate::midend::{
    ir::{unlowered::Lowerable, *},
    linearizer::GenericParamsContext,
    *,
};
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

pub fn lower_function(
    def_path: symtab::DefPath,
    mut symtab: Box<symtab::SymbolTable>,
) -> Box<symtab::SymbolTable> {
    let _span = trace::span_auto_debug!("Lower function ", "{}", def_path.last());

    loop {
        let (mut cf, mut unlowered, function_name) = {
            let function = symtab.lookup_at_mut::<symtab::Function>(&def_path).unwrap();

            match &mut function.control_flow {
                Some(cf) => {
                    trace::trace!("run type inference");

                    let unlowered = find_unlowered_irs(&cf);
                    if unlowered.len() == 0 {
                        trace::trace!("No unlowered statements in control flow, skipping");
                        break;
                    }
                    (
                        function.control_flow.take().unwrap(),
                        unlowered,
                        symtab::FunctionName::new(function.name().into()),
                    )
                }
                None => {
                    trace::trace!("No control flow, nothing to lower.");
                    break;
                }
            }
        };

        symtab = cf.infer_types(symtab).1;

        trace::trace!("the following statements need lowering: {:?}", unlowered);

        let mut manager: BlockManager = cf.into();

        let block = *unlowered.keys().next().unwrap();
        let idx = *unlowered.get(&block).unwrap().iter().next().unwrap();

        let (split_to_block, to_lower) = manager.split_block_at_statement(block, idx).unwrap();

        let after_split_block = manager
            .finish_block_split(split_to_block, to_lower.loc.clone())
            .unwrap();

        let mut ctx = linearizer::WalkContext::from_existing(
            symtab,
            GenericParamsContext::new(),
            def_path.clone(),
            manager,
            block,
        );

        match to_lower.operation {
            ir::Operation::Unlowered(op) => {
                op.lower(&mut ctx, to_lower.loc.clone());
            }
            ir::Operation::Lowered(_) => panic!("Can't lower lowered IR"),
        }

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

        ctx.finish_function(function_name.clone()).unwrap();
        symtab = ctx.take().unwrap().0;
    }

    symtab
}
