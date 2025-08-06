use serde::{Deserialize, Serialize};

#[derive(Copy, Clone, Debug, Serialize, Deserialize, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub struct Semantic {
    pub id: usize,
}

impl std::fmt::Display for Semantic {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.id)
    }
}
