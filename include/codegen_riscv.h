#include "substratum_defs.h"

struct CodegenState;
struct MachineInfo;
struct Register;
struct TACLine;
struct TACOperand;
struct RegallocMetadata;
struct MachineInfo;
struct BasicBlock;
struct Scope;

void riscv_emit_frame_store_for_size(struct TACLine *correspondingTACLine,
                                     struct CodegenState *state,
                                     struct MachineInfo *info,
                                     struct Register *sourceReg,
                                     u8 size,
                                     ssize_t offset);

void riscv_emit_frame_load_for_size(struct TACLine *correspondingTACLine,
                                    struct CodegenState *state,
                                    struct MachineInfo *info,
                                    struct Register *destReg,
                                    u8 size,
                                    ssize_t offset);

void riscv_emit_stack_store_for_size(struct TACLine *correspondingTACLine,
                                     struct CodegenState *state,
                                     struct MachineInfo *info,
                                     struct Register *sourceReg,
                                     u8 size,
                                     ssize_t offset);

void riscv_emit_stack_load_for_size(struct TACLine *correspondingTACLine,
                                    struct CodegenState *state,
                                    struct MachineInfo *info,
                                    struct Register *destReg,
                                    u8 size,
                                    ssize_t offset);

void riscv_emit_push_for_size(struct TACLine *correspondingTACLine,
                              struct CodegenState *state,
                              u8 size,
                              struct Register *srcRegister);

void riscv_emit_pop_for_size(struct TACLine *correspondingTACLine,
                             struct CodegenState *state,
                             u8 size,
                             struct Register *destRegister);

void riscv_emit_prologue(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info);

void riscv_emit_epilogue(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info, char *functionName);

struct Register *riscv_place_or_find_operand_in_register(struct TACLine *correspondingTACLine,
                                                         struct CodegenState *state,
                                                         struct RegallocMetadata *metadata,
                                                         struct MachineInfo *info,
                                                         struct TACOperand *operand,
                                                         struct Register *optionalScratch);

void riscv_write_variable(struct TACLine *correspondingTACLine,
                          struct CodegenState *state,
                          struct RegallocMetadata *metadata,
                          struct MachineInfo *info,
                          struct TACOperand *writtenTo,
                          struct Register *dataSource);

// place a literal in the register specified by numerical index, return string of register's name for asm
void riscv_place_literal_string_in_register(struct TACLine *correspondingTACLine,
                                            struct CodegenState *state,
                                            char *literalStr,
                                            struct Register *destReg);

void riscv_place_literal_value_in_register(struct TACLine *correspondingTACLine,
                                           struct CodegenState *state,
                                           size_t literalVal,
                                           struct Register *destReg);

void riscv_place_addr_of_operand_in_reg(struct TACLine *correspondingTACLine,
                                        struct CodegenState *state,
                                        struct RegallocMetadata *metadata,
                                        struct MachineInfo *info,
                                        struct TACOperand *operand,
                                        struct Register *destReg);

void riscv_generate_code_for_basic_block(struct CodegenState *state,
                                         struct RegallocMetadata *metadata,
                                         struct MachineInfo *info,
                                         struct BasicBlock *block,
                                         char *functionName);
