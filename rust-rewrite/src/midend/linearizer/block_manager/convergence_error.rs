#[derive(Debug, PartialEq, Eq)]
pub enum ConvergenceError {
    FromBlockExists(usize),
    ToBlockExists(usize),
    NonexistentFrom(usize),
}
