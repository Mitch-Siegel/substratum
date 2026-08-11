use crate::midend::{
    ir::ControlFlow,
    symtab::{self, Path, Symtab},
    trace,
};
use std::collections::{HashMap, HashSet};

pub(crate) fn find_unlowered_irs(cf: &ControlFlow) -> HashMap<usize, HashSet<usize>> {
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

pub(crate) fn lower_function(
    def_path: &symtab::ValuePath,
    mut symtab: symtab::SymbolTable,
) -> symtab::SymbolTable {
    let _span = trace::span_auto_debug!("Lower function ", "{}", def_path.last());

    #[allow(clippy::never_loop)]
    loop {
        let (mut cf, unlowered, _function_name) = {
            let symtab::Value::Function(function) = symtab.lookup_value_at_mut(def_path).unwrap()
            else {
                panic!("expected {def_path:?} to be a function")
            };

            if let Some(cf) = &mut function.control_flow {
                trace::trace!("run type inference");

                let unlowered = find_unlowered_irs(cf);
                if unlowered.is_empty() {
                    trace::trace!("No unlowered statements in control flow, skipping");
                    break;
                }
                (
                    function.control_flow.take().unwrap(),
                    unlowered,
                    function.name(),
                )
            } else {
                trace::trace!("No control flow, nothing to lower.");
                break;
            }
        };

        cf.infer_types(&mut symtab);

        trace::trace!("the following statements need lowering: {:?}", unlowered);

        unimplemented!();

        /*
        let mut manager: BlockManager = cf.into();

        let block = *unlowered.keys().next().unwrap();
        let idx = *unlowered.get(&block).unwrap().iter().next().unwrap();

        let (split_to_block, to_lower) = manager.split_block_at_statement(block, idx).unwrap();

        let after_split_block = manager
            .finish_block_split(split_to_block, to_lower.loc.clone())
            .unwrap();

        let mut dummy_generics = GenericParamsContext::new();
        let mut dummy_def_path = def_path.clone();
        while dummy_def_path.len() > 0 {
            dummy_generics
                .add_params_at_path(dummy_def_path.clone(), BTreeSet::new())
                .unwrap();
            dummy_def_path.pop().unwrap();
        }

        let mut ctx = treewalk::LinearizeCtx::from_existing(
            symtab,
            dummy_generics,
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
        */

        // ctx.refinish_function(function_name.clone()).unwrap();
        // symtab = ctx.take().unwrap().0;
    }

    symtab
}
