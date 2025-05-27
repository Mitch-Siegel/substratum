mod backend;
mod frontend;
mod midend;

mod hashmap_ooo_iter;

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

const NESTED_WHILE_LOOPS: &str = "
fun while_with_nested_branch(u8 a, u16 b, u32 c) {
    while (a < b) {
        u8 counter;
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

fn main() {
    println!("Hello, world!");
    let mut parser = Parser::new(Lexer::from_string(STRUCT_EXAMPLE));
    let program = parser.parse().expect("Error parsing input");

    for t in &program {
        println!("{}", t);
    }

    let mut symtab = midend::symbol_table_from_program(program);

    // println!("{}", serde_json::to_string_pretty(&symtab).unwrap());
    println!("SYMTAB IR");
    symtab.print_ir();

    symtab.assign_program_points();

    generate_code(symtab);
}
