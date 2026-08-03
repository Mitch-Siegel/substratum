od trace;

mod backend;
mod frontend;
mod midend;

mod args;
mod map_ooo_iter;
pub(crate) use args::Config;

//use backend::generate_code;
mod example_inputs {
    #![allow(dead_code)]
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
}

fn main() {
    let cfg = Config::parse();

    println!("{:?}", cfg);

    match &cfg.trace_file {
        None => {
            tracing_subscriber::fmt()
                //.event_format(trace::Print::default())
                .pretty()
                .with_writer(std::io::stdout)
                .with_max_level(cfg.trace_level)
                .init();
        }
        Some(outfile_name) => {
            let outfile = std::fs::File::create(std::path::Path::new(outfile_name))
                .expect(&format!("couldn't create trace file '{}'", outfile_name));
            let json_outfile = std::sync::Mutex::new(outfile);
            let writer = tracing_subscriber::fmt::writer::BoxMakeWriter::new(json_outfile);
            tracing_subscriber::fmt()
                .with_writer(writer)
                .with_max_level(cfg.trace_level)
                .init();
        }
    }

    assert!(
        !cfg.input_files.is_empty(),
        "input file must be provided! (-i [file])"
    );

    // let mut modules = BTreeSet::<frontend::ast::ModuleTree>::new();

    let modules =
        frontend::parser::parse_crate(&cfg.crate_name, &cfg.bin_name, cfg.crate_path).unwrap();

    let _symtab = midend::symbol_table_from_modules(modules, &cfg.crate_name);
    //backend::do_backend(symtab);
}
