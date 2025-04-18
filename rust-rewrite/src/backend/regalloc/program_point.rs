use core::fmt;

use serde::Serialize;

#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Debug, Serialize, Hash)]
pub struct ProgramPoint {
    pub depth: usize, // depth in the DFS traversal of control flow
    pub index: usize, // index within a basic block
}

impl ProgramPoint {
    pub fn default() -> Self {
        Self::new(0, 0)
    }

    pub fn new(depth: usize, index: usize) -> Self {
        ProgramPoint { depth, index }
    }
}

impl std::fmt::Display for ProgramPoint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:>2x}:{:<2x}", self.depth, self.index)
    }
}

#[cfg(test)]
mod tests {
    use std::cmp::Ordering;

    use crate::backend::regalloc::program_point::ProgramPoint;

    #[test]
    fn test_default() {
        let default_point = ProgramPoint::default();

        assert_eq!(default_point.depth, 0);
        assert_eq!(default_point.index, 0);
    }

    #[test]
    fn test_partial_ord_eq() {
        let point_1 = ProgramPoint::new(1, 1);
        let point_2 = ProgramPoint::new(1, 0);
        let point_3 = ProgramPoint::new(1, 1);

        assert_eq!(point_1.partial_cmp(&point_2), Some(Ordering::Greater));
        assert_eq!(point_1.partial_cmp(&point_3), Some(Ordering::Equal));
    }

    #[test]
    fn test_ord() {
        let point_1 = ProgramPoint::new(1, 1);
        let point_2 = ProgramPoint::new(1, 0);
        let point_3 = ProgramPoint::new(1, 1);

        assert_eq!(point_1.cmp(&point_2), Ordering::Greater);
        assert_eq!(point_1.cmp(&point_3), Ordering::Equal);
    }
}
