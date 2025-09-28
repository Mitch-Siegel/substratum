use crate::midend::{ir::type_inference::*, ir::*, *};

impl OperandTypeInference for BlockManager {
    fn infer_types(&self, ctx: &TypeInferenceContext) -> bool {
        let mut lines_to_infer = usize::MAX;
        loop {
            let remaining_to_infer = self.
        }
    }
}
