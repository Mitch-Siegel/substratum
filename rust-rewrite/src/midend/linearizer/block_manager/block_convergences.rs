use std::collections::HashMap;

use crate::{midend::linearizer::block_manager::*, trace};

#[derive(Debug, PartialEq, Eq)]
pub enum ConvergenceResult {
    NotDone(usize),       // the label of the block to converge to
    Done(ir::BasicBlock), // the block converged to
}

#[derive(Debug)]
pub struct BlockConvergences {
    open_convergences: HashMap<usize, usize>,
    convergence_blocks: HashMap<usize, ir::BasicBlock>,
}

impl BlockConvergences {
    pub fn new() -> Self {
        Self {
            open_convergences: HashMap::new(),
            convergence_blocks: HashMap::new(),
        }
    }

    pub fn add(&mut self, froms: &[usize], to: ir::BasicBlock) -> Result<(), ConvergenceError> {
        trace::trace!("add convergence from {:?} to {}", froms, to.label);
        for from in froms {
            match self.open_convergences.insert(*from, to.label) {
                Some(_) => return Err(ConvergenceError::FromBlockExists(*from)),
                None => (),
            }
        }

        match self.convergence_blocks.insert(to.label, to) {
            Some(block) => Err(ConvergenceError::ToBlockExists(block.label)),
            None => Ok(()),
        }
    }

    pub fn converge(&mut self, from: usize) -> Result<ConvergenceResult, ConvergenceError> {
        let converge_to = match self.open_convergences.remove(&from) {
            Some(label) => label,
            None => return Err(ConvergenceError::NonexistentFrom(from)),
        };

        let remaining_with_same_target = self
            .open_convergences
            .iter()
            .map(|(_, to)| if *to == converge_to { 1 } else { 0 })
            .sum::<usize>();

        if remaining_with_same_target > 0 {
            trace::trace!(
                "converge from block {} to {} ({} open convergence(s) still target {})",
                from,
                converge_to,
                remaining_with_same_target,
                converge_to
            );
            Ok(ConvergenceResult::NotDone(converge_to))
        } else {
            trace::trace!(
                "block {} converges to {} (no remaining convergences targeting {})",
                from,
                converge_to,
                converge_to
            );
            // manually unwrap and rewrap to ensure we actually removed something
            Ok(ConvergenceResult::Done(
                self.convergence_blocks.remove(&converge_to).unwrap(),
            ))
        }
    }

    #[tracing::instrument(skip(self))]
    pub fn rename_source(&mut self, old: usize, new: usize) {
        self.open_convergences = self
            .open_convergences
            .iter()
            .map(|(from, to)| {
                if *from == old {
                    trace::trace!(
                        "Rename convergence ({}->{}) to ({}->{})",
                        from,
                        *to,
                        new,
                        *to
                    );
                    (new, *to)
                } else {
                    (*from, *to)
                }
            })
            .collect::<HashMap<usize, usize>>();
    }

    pub fn is_empty(&self) -> bool {
        self.open_convergences.is_empty() && self.convergence_blocks.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use crate::midend::{ir, linearizer::*};
    use std::collections::HashMap;

    #[test]
    fn add() {
        let mut c = BlockConvergences::new();
        assert_eq!(c.add(&[0, 1, 2], ir::BasicBlock::new(3)), Ok(()));

        let expected_convergences = [(0, 3), (1, 3), (2, 3)]
            .into_iter()
            .collect::<HashMap<usize, usize>>();

        assert_eq!(c.open_convergences, expected_convergences);
        assert_eq!(c.convergence_blocks.len(), 1);
        assert_eq!(c.convergence_blocks.get(&3).unwrap().label, 3);

        assert_eq!(
            c.add(&[4], ir::BasicBlock::new(3)),
            Err(ConvergenceError::ToBlockExists(3))
        );

        assert_eq!(
            c.add(&[0], ir::BasicBlock::new(4)),
            Err(ConvergenceError::FromBlockExists(0))
        );
    }

    #[test]
    fn converge_and_is_empty() {
        let mut c = BlockConvergences::new();
        assert_eq!(c.add(&[0, 1, 2], ir::BasicBlock::new(3)), Ok(()));
        assert_eq!(c.is_empty(), false);

        assert_eq!(c.converge(4), Err(ConvergenceError::NonexistentFrom(4)));

        assert_eq!(c.converge(2), Ok(ConvergenceResult::NotDone(3)));
        assert_eq!(c.converge(0), Ok(ConvergenceResult::NotDone(3)));
        assert_eq!(
            c.converge(1),
            Ok(ConvergenceResult::Done(ir::BasicBlock::new(3)))
        );
        assert_eq!(c.is_empty(), true);

        assert_eq!(c.converge(0), Err(ConvergenceError::NonexistentFrom(0)));
    }

    #[test]
    fn rename_source() {
        let mut c = BlockConvergences::new();
        assert_eq!(c.add(&[0, 1, 2], ir::BasicBlock::new(3)), Ok(()));

        c.rename_source(0, 4);

        let expected_convergences = [(4, 3), (1, 3), (2, 3)]
            .into_iter()
            .collect::<HashMap<usize, usize>>();

        assert_eq!(c.open_convergences, expected_convergences);
    }
}
