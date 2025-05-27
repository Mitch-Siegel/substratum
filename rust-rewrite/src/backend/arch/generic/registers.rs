use std::collections::BTreeMap;

pub enum RegisterSaveConvention {
    CallerSave,
    CalleeSave,
    NoSave,
}

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
    registers_by_name: BTreeMap<String, Register>,
}

impl ArchitectureRegisters {
    pub fn new() -> Self {
        Self {
            registers_by_name: BTreeMap::new(),
        }
    }

    pub fn add(&mut self, register: Register) {
        self.registers_by_name
            .insert(register.name.clone(), register);
    }
}
