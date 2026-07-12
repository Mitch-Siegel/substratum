pub(crate) mod block_args;
mod idfa_base;
pub(crate) mod live_vars;
pub(crate) mod reaching_defs;

pub(crate) use idfa_base::{Facts, IdfaImplementor};
