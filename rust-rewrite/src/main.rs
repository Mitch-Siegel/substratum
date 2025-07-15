#![allow(dead_code)]

use std::collections::BTreeSet;

mod trace;

mod backend;
mod frontend;
mod midend;

mod map_ooo_iter;

//use backend::generate_code;

const FIB_FUN: &str = "fun fib(u8 n) -> u64
{
    u64 result;
    result = 0;
    if (n > 0) {
        if(n == 0) {
            result = 0;
        } else {
         result = 1;
        }
    } else {
        result = (n - 1) - (n - 2);
    }
}";

const WHILE_LOOP: &str = "fun down_to_zero(input: u16)
{
    input = input;
    while(input > 0) {
        input = input - 1;
    }

    input = input + 1;
}";

const WHILE_LOOP_WITH_NESTED_BRANCH: &str = "
fun while_with_nested_branch(a: u8, b: u16, c: u32) {
    counter: u8;
    counter = 0;
    while (a < b) {
        if (c > 22) {
            counter = counter + 1;
        } else {
            counter = counter - 1;
        }
    }

    a = a + b;
}
";

const WHILE_LOOP_WITH_NESTED_BRANCH_NO_ARGS: &str = "
fun while_with_nested_branch() {
    a: u8; b: u16; c: u32;

    a = 0;
    b = 1;
    c = 2;
    while (a < b) {
        counter: u8;
        if (c > 22) {
            a = a + b;
        } else {
            b = b - 1;
        }
    }
}
";

const NESTED_WHILE_LOOPS: &str = "
fun while_with_nested_branch(a: u8, b: u64, c: u32) {
    while (a < b) {
        counter: u8;
        counter = 0;
        if (c > 22) {
            counter = counter + 1;
        } else {
            while (counter > 0) {
            counter = counter - 1;
            }
        }
    }

    a = a + b;
}
";

const SSA_EXAMPLE: &str = "
fun while_with_nested_branch() {
    a: u8; b: u16; c: u32;

    a = 0;
    b = 1;
    c = 2;
    
    a = b + c;
    b = a + c;
    if (a > b) {
    c = c + 1;
    } else {
     c = 1;}
    c = c + 1;
    c = c + 1;
}";

const STRUCT_EXAMPLE: &str = "struct Money {
dollars: u64,
cents: u8
}

impl Money {
    fun new(dollars: u64, cents: u8) -> Self {
    }

    fun print(&self) {
    }
}

fun money_add_dollars(m: Money, dollars: u64) {
    m.dollars = m.dollars + dollars;
    m.print();
}";

#[derive(Debug)]
enum TraceLocation {
    NoTrace,
    Stdout,
    File,
}

#[derive(Debug)]
struct CompilerArguments {
    trace_location: TraceLocation,
    trace_level: tracing::Level,
}

fn file_path_to_module_name(filepath_to_parse: &std::path::Path) -> (String, &std::path::Path) {
    let stem: String = filepath_to_parse
        .file_stem()
        .unwrap()
        .to_str()
        .unwrap()
        .into();
    let module_name: String = match stem.as_str() {
        "mod" | "lib" => filepath_to_parse.parent().unwrap().to_str().unwrap().into(),
        _ => stem,
    };
    (
        module_name,
        filepath_to_parse
            .parent()
            .unwrap_or(std::path::Path::new("")),
    )
}

fn main() {
    println!("Hello, world!");

    let mut arguments = CompilerArguments {
        trace_location: TraceLocation::NoTrace,
        trace_level: tracing::Level::WARN,
    };

    let mut args_without_executable = std::env::args().map(|arg| arg).collect::<Vec<_>>();
    args_without_executable.remove(0);

    let mut input_file = String::new();

    // TODO: real argument parsing
    for argument in args_without_executable {
        match argument.as_str() {
            "trace_file" => arguments.trace_location = TraceLocation::File,
            "trace_stdout" => arguments.trace_location = TraceLocation::Stdout,
            "trace_level_trace" => arguments.trace_level = tracing::Level::TRACE,
            "trace_level_debug" => arguments.trace_level = tracing::Level::DEBUG,
            "trace_level_info" => arguments.trace_level = tracing::Level::INFO,
            "trace_level_warn" => arguments.trace_level = tracing::Level::WARN,
            "trace_level_error" => arguments.trace_level = tracing::Level::ERROR,
            input_file_argument => input_file = input_file_argument.into(),
        }
        println!("{}", argument);
    }

    println!("{:?}", arguments);

    match arguments.trace_location {
        TraceLocation::NoTrace => (),
        TraceLocation::Stdout => {
            tracing_subscriber::fmt()
                //.event_format(trace::Print::default())
                .pretty()
                .with_writer(std::io::stdout)
                .with_max_level(arguments.trace_level)
                .init();
        }
        TraceLocation::File => {
            let outfile = std::fs::File::create("most_recent").expect("Couldn't create trace file");
            let json_outfile = std::sync::Mutex::new(outfile);
            let writer = tracing_subscriber::fmt::writer::BoxMakeWriter::new(json_outfile);
            tracing_subscriber::fmt()
                .with_writer(writer)
                .with_max_level(arguments.trace_level)
                .init();
        }
    }

    assert!(input_file.len() > 0, "Input file must be provided!");
    let mut module_worklist = BTreeSet::<String>::new();
    let (_, worklist_item) = file_path_to_module_name(&std::path::Path::new(&input_file));
    module_worklist.insert(worklist_item.to_str().unwrap().into());
    let mut modules = Vec::<frontend::ast::ModuleTree>::new();

    while module_worklist.len() > 0 {
        let filename_to_parse = module_worklist.pop_last().unwrap();
        let filepath_to_parse = std::path::Path::new(&filename_to_parse);

        let (input_file, module_path, _opened_path, module_name) = {
            let _ = trace::span_auto_debug!("Parse worklist item {}", filename_to_parse);
            let (module_name, module_parent_path) = file_path_to_module_name(&filepath_to_parse);

            trace::debug!(
                "Module name: \"{}\", parent path: \"{}\"",
                module_name,
                module_parent_path.display()
            );

            // first, try from [worklist_item].sb
            let direct_source_file = module_parent_path
                .join(module_name.clone())
                .with_extension("sb");
            trace::trace!("Try file \"{}\"", direct_source_file.display());

            let (opened_file, opened_path) =
                if let Ok(file) = std::fs::File::open(direct_source_file.clone()) {
                    trace::trace!("success");
                    (file, direct_source_file)
                } else {
                    // then, try from [worklist_item]/mod.sb
                    let module_source_file = module_parent_path
                        .join(module_name.clone())
                        .join("mod")
                        .with_extension("sb");
                    trace::trace!("Try file \"{}\"", module_source_file.display());

                    if let Ok(file) = std::fs::File::open(module_source_file.clone()) {
                        (file, module_source_file)
                    } else {
                        let library_source_file = module_parent_path
                            .join(module_name.clone())
                            .join("lib")
                            .with_extension("sb");
                        trace::trace!("Try file \"{}\"", library_source_file.display());
                        (
                            std::fs::File::open(library_source_file.clone()).unwrap(),
                            library_source_file,
                        )
                    }
                };

            (opened_file, module_parent_path, opened_path, module_name)
        };

        trace::debug!(
            "Next up from module worklist: {} - module name {}",
            filename_to_parse,
            module_name
        );
        let mut parser = frontend::Parser::new(
            module_name.clone(),
            module_path,
            frontend::Lexer::from_file(&filepath_to_parse, std::fs::File::from(input_file)),
        );

        let frontend::parser::ModuleResult {
            module_tree,
            module_worklist: mut parsed_worklist,
        } = parser
            .parse(module_path, module_name)
            .expect(&format!("Error in file {}", filename_to_parse));

        module_worklist.append(&mut parsed_worklist);

        for t in &module_tree.items {
            println!("{}", t);
        }

        modules.push(module_tree);
    }

    let symtab = midend::symbol_table_from_modules(modules);
    //println!("{}", serde_json::to_string_pretty(&symtab).unwrap());
    println!("{:?}", &symtab);
    //backend::do_backend(symtab);
}
