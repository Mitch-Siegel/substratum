use crate::{frontend, trace};

mod idfa;
pub mod ir;
pub mod linearizer;
mod optimization;
//mod ssa_gen;
pub mod symtab;
pub mod types;

fn functions_to_graphviz(symtab: &symtab::SymbolTable, suffix: String) {
    let _ = symtab::Visitor::visit_with_starting_data(
        symtab,
        |_path, symbol, suffix| match symbol {
            symtab::SymbolDef::Function(f) => {
                if let Some(cf) = &f.control_flow {
                    {
                        use std::io::Write;
                        let path_string = format!("graphviz/{}{}.txt", f.name(), suffix);
                        let filepath = std::path::Path::new(&path_string);
                        std::fs::create_dir_all(filepath.parent().unwrap()).unwrap();
                        let mut file = std::fs::File::create(filepath).unwrap();
                        file.write(cf.graphviz_string().as_bytes()).unwrap();
                    }
                } else {
                    panic!();
                }
            }
            _ => (),
        },
        suffix,
    );
}

pub fn symbol_table_from_modules(modules: Vec<frontend::ast::ModuleTree>) -> symtab::SymbolTable {
    let _ = trace::span_auto!(trace::Level::DEBUG, "Generate symbol table from AST");

    tracing::debug!("Linearize");
    let mut symtab = linearizer::linearize(modules);

    functions_to_graphviz(&symtab, "_unlowered".into());

    symtab = ir::lowering::lower_symtab(symtab);
    ir::lowering::assert_lowered(&symtab);
    functions_to_graphviz(&symtab, "".into());

    //tracing::debug!("collapse scopes");
    //symtab.collapse_scopes();

    tracing::debug!("convert IR to SSA");
    //ssa_gen::convert_functions_to_ssa(&mut symtab);

    // optimization::optimize_functions(&mut symtab.functions);
    //

    tracing::debug!("convert IR back from SSA");
    //ssa_gen::remove_ssa_from_functions(&mut symtab);

    *symtab
}
