mod function_linearize_context;
pub mod linearize_context;

pub use function_linearize_context::FunctionLinearizeCtx;
pub use linearize_context::{GenericParamsContext, LinearizeCtx};

pub trait Linearize<T> {
    fn linearize(self, context: &mut LinearizeCtx) -> T;
}
