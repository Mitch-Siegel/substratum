use std::collections::BTreeMap;

pub enum RegisterSaveConvention {
    CallerSave,
    CalleeSave,
    NoSave,
}

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum RegisterPurpose {
    GeneralPurpose,
    Argument,
    Temporary,
    FramePointer,
    StackPointer,
    ReturnAddress,
    Zero,
    Other,
}

pub struct Register {
    pub name: String,
    pub purpose: RegisterPurpose,
    pub save: RegisterSaveConvention,
}

impl Register {
    pub fn new(name: &str, purpose: RegisterPurpose, save: RegisterSaveConvention) -> Self {
        Self {
            name: String::from(name),
            purpose,
            save,
        }
    }
}

pub struct ArchitectureRegisters {
    pub registers_by_name: BTreeMap<String, Register>,
    pub counts_by_purpose: BTreeMap<RegisterPurpose, usize>,
}

impl ArchitectureRegisters {
    pub fn new() -> Self {
        Self {
            registers_by_name: BTreeMap::new(),
            counts_by_purpose: BTreeMap::new(),
        }
    }

    pub fn add(&mut self, register: Register) {
        *(self.counts_by_purpose.entry(register.purpose).or_default()) += 1;

        self.registers_by_name
            .insert(register.name.clone(), register);
    }
}
