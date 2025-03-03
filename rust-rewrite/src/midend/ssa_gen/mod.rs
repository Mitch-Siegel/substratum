use super::ir;

fn convert_block_writes_to_ssa(block: &ir::NonSsaBlock, new_cfg: &mut Vec<ir::SsaBlock>) {}

fn convert_writes_to_ssa(blocks: &Vec<ir::NonSsaBlock>) -> Vec<ir::SsaBlock> {
    let mut new_cfg = Vec::<ir::SsaBlock>::new();

    for block in blocks {
        convert_block_writes_to_ssa(block, &mut new_cfg);
    }

    new_cfg
}

pub fn convert_flow_to_ssa(mut control_flow: ir::ControlFlow) -> ir::ControlFlow {
    convert_writes_to_ssa(control_flow.blocks.as_basic_mut());

    let mut new_flow = ir::ControlFlow::new();

    new_flow
}
