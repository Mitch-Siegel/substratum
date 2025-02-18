use core::fmt;
use std::cmp::Ordering;

use serde::Serialize;

#[derive(Copy, Clone, Debug, Serialize, Hash)]
pub struct ProgramPoint {
    pub depth: usize, // depth in the BFS traversal of control flow
    pub index: usize, // index within a basic block
}

impl PartialOrd for ProgramPoint {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(
            self.depth
                .cmp(&other.depth)
                .then(self.index.cmp(&other.index)),
        )
    }
}

impl PartialEq for ProgramPoint {
    fn eq(&self, other: &Self) -> bool {
        (self.depth == other.depth) && (self.index == other.index)
    }
}

impl Eq for ProgramPoint {}

impl Ord for ProgramPoint {
    fn cmp(&self, other: &Self) -> Ordering {
        self.depth
            .cmp(&other.depth)
            .then(self.index.cmp(&other.index))
    }
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

    use crate::midend::program_point::ProgramPoint;

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
