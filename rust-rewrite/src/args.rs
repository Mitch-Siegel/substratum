use std::iter::Peekable;

use std::str::FromStr;

macro_rules! arg_struct {
    ($T:ident {
        $($a:tt => $field:ident: $ty_:ty where default = $data:expr, parse_fn = $parse:expr),* $(,)?
    }) => {
        #[derive(Debug)]
        pub(crate) struct $T {
            $(pub $field: $ty_),*
        }

        impl Default for $T {
            fn default() -> Self {
                Self {
                    $($field: $data),*
                }
            }
        }

        impl $T {
            pub(self) fn parse_one<I: Iterator<Item = String>>(&mut self, args: &mut Peekable<I>) -> bool {
                let Some(arg) = args.peek() else {
                    return false;
                };
                match arg.as_str() {
                    $(
                        $a => {
                            let _ = args.next();
                            let parse_fn: &mut dyn FnMut(&mut $T, &mut Peekable<I>) -> bool = $parse;
                            parse_fn(self, args)
                        },
                    )*
                    _ => false,
                }
            }
        }
    }
}

arg_struct! {
    Config {
        "-crate-name" => crate_name: String where default = String::from("crate"), parse_fn = &mut |config, args| {
            config.crate_name = args.next().expect("expected a value");
            true
        },
        "-trace-file" => trace_file: Option<String> where default = None, parse_fn = &mut |config, args| {
            config.trace_file = Some(args.next().expect("expected a value"));
            true
        },
        "-trace-level" => trace_level: tracing::Level where default = tracing::Level::ERROR, parse_fn = &mut |config, args| {
            config.trace_level = tracing::Level::from_str(&args.next().expect("expected a value")).expect("malformed trace level string");
            true
        },
        "-i" => input_files: Vec<String> where default = vec![], parse_fn = &mut |config, args| {
            config.input_files.push(args.next().expect("expected a value"));
            true
        }
    }
}

impl Config {
    pub(crate) fn parse() -> Self {
        let mut args = std::env::args().skip(1).peekable();
        let mut settings = Self::default();
        while args.peek().is_some() {
            if !settings.parse_one(&mut args) {
                panic!("unknown argument {:?}", args.peek().unwrap());
            }
        }
        settings
    }
}
