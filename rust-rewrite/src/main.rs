mod backend;
mod frontend;
mod midend;

use backend::generate_code;
use frontend::{lexer::Lexer, parser::Parser};

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

const WHILE_LOOP: &str = "fun down_to_zero(u16 input)
{
    input = input;
    while(input > 0) {
        input = input - 1;
    }

    input = input + 1;
}";

fn main() {
    println!("Hello, world!");
    let parsed = String::from(WHILE_LOOP);
    let mut parser = Parser::new(Lexer::new(parsed.chars()));
    let program = parser.parse();

    for t in &program {
        println!("{}", t);
    }

    let mut symtab = midend::symbol_table_from_program(program);

    // println!("{}", serde_json::to_string_pretty(&symtab).unwrap());
    symtab.print_ir();

    symtab.assign_program_points();

    generate_code(symtab);
}
