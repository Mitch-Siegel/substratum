use crate::{frontend, trace};

use std::collections::BTreeSet;

#[allow(unused)]
mod idfa;
pub mod ir;
mod optimization;
pub mod treewalk;
//mod ssa_gen;
pub mod symtab;
pub mod types;

fn functions_to_graphviz(symtab: &symtab::SymbolTable, suffix: String) {
    let _ = symtab::Visitor::visit_with_starting_data(
        symtab,
        |_path, symbol, suffix| {
            if let symtab::SymbolDef::Value(symtab::Value::Function(f)) = symbol {
                if let Some(cf) = &f.control_flow {
                    {
                        use std::io::Write;
                        let path_string = format!("graphviz/{}{}.txt", f.name(), suffix);
                        let filepath = std::path::Path::new(&path_string);
                        std::fs::create_dir_all(filepath.parent().unwrap()).unwrap();
                        let mut file = std::fs::File::create(filepath).unwrap();
                        file.write_all(cf.graphviz_string().as_bytes()).unwrap();
                    }
                } else {
                    panic!();
                }
            }
        },
        suffix,
    );
}

pub fn symbol_table_from_modules(
    modules: BTreeSet<frontend::ast::ModuleTree>,
) -> symtab::SymbolTable {
    let _ = trace::span_auto!(trace::Level::DEBUG, "Generate symbol table from AST");

    tracing::debug!("Walk AST");
    let mut symtab = treewalk::walk(modules);

    functions_to_graphviz(&symtab, "_unlowered".into());

    for path in symtab.decls() {
        println!("{}", path);
    }

    for (path, instances) in symtab.types.all_monomorphizations() {
        println!("{}", path);
        for i in instances {
            println!("\t{:?}", i);
        }
    }
    println!("done printing instances");

    /*
    let all_arguments = symtab::Visitor::visit(&symtab, get_all_function_arguments);

    monomorphization::monomorphize_generics(&mut symtab);
    assign_types_to_function_arguments(&mut symtab, all_arguments);
    */
    symtab = ir::lowering::lower_symtab(symtab);
    ir::lowering::assert_lowered(&symtab);

    functions_to_graphviz(&symtab, "".into());

    //tracing::debug!("collapse scopes");
    //symtab.collapse_scopes();

    //tracing::debug!("convert IR to SSA");
    //ssa_gen::convert_functions_to_ssa(&mut symtab);

    // optimization::optimize_functions(&mut symtab.functions);
    //

    //tracing::debug!("convert IR back from SSA");
    //ssa_gen::remove_ssa_from_functions(&mut symtab);

    symtab
}
