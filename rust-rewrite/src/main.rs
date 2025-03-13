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

const WHILE_LOOP_WITH_NESTED_BRANCH: &str = "
fun while_with_nested_branch(u8 a, u16 b, u32 c) {
    while (a < b) {
        u8 counter;
        counter = 0;
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
    u8 a; u16 b; u32 c;

    a = 0;
    b = 1;
    c = 2;
    while (a < b) {
        u8 counter;
        if (c > 22) {
            a = a + b;
        } else {
            b = b - 1;
        }
    }
}
";

const SSA_EXAMPLE: &str = "
fun while_with_nested_branch() {
    u8 a; u16 b; u32 c;

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

fn main() {
    println!("Hello, world!");
    let parsed = String::from(WHILE_LOOP_WITH_NESTED_BRANCH);
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
