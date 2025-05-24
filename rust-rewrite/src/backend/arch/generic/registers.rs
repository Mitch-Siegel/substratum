
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
}

pub struct Register {
    pub name: String,
    pub purpose: RegisterPurpose,
    pub save: RegisterSaveConvention,
}

impl Register {
    pub fn new(name: &str, purpose: RegisterPurpose, save: RegisterSaveConvention) -> Self {
        Self {
            name,
            purpose,
            save,
        }
}

pub struct ArchitectureRegisters {
    
}

