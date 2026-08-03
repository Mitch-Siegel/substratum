pub trait NameReflectable {
    fn reflect_name() -> &'static str;
}

pub use name_derive_macro::ReflectName;
