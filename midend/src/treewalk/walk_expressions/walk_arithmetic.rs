use frontend::ast;

use crate::{
    symtab,
    treewalk::{Collect, CollectCtx, CollectResult},
};

impl Collect<symtab::ValuePath> for ast::expressions::arithmetic::ArithmeticDualOperands {
    fn collect_inner(&self, mut ctx: CollectCtx<symtab::ValuePath>) -> CollectResult {
        ctx = self.e1.collect_symbols(ctx)?;
        self.e2.collect_inner(ctx)
    }
}
