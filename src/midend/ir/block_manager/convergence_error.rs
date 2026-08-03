#[derive(Debug, PartialEq, Eq)]
pub(crate) enum ConvergenceError {
    FromBlockExists(usize),
    ToBlockExists(usize),
    NonexistentFrom(usize),
    NonexistentTo(usize),
}
