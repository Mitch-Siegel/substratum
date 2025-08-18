use serde::Serialize;

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum Operation {}

impl std::fmt::Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        unimplemented!();
    }
}
