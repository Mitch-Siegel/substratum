use proc_macro::TokenStream;
use quote::quote;
use syn::{DeriveInput, parse_macro_input};

#[proc_macro_derive(ReflectName)]
pub fn derive_name(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;
    let name_str = name.to_string();

    let expanded = quote! {
        impl NameReflectable for #name {
            fn reflect_name() -> &'static str {
                #name_str
            }
        }
    };

    TokenStream::from(expanded)
}
