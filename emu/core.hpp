#include <cstdint>
#include <string>

#include "memory.hpp"
#include "faults.hpp"

#ifndef _CORE_HPP_
#define _CORE_HPP_

const static std::string registerNames[16] = {
    "%r0", "%r1", "%r2", "%r3", "%r4", "%r5", "%r6", "%r7", "%r8", "%r9", "%ra", "%rb", "%rc", "%rr", "%sp", "%bp"};

enum Registers
{
    r0,
    r1,
    r2,
    r3,
    r4,
    r5,
    r6,
    r7,
    r8,
    r9,
    r10,
    r11,
    r12,
    r13,
    sp,
    bp
};

const static std::string configRegisterNames[16] = {
    "%ip",
    "cid",
    "ptbr",
    "palim",
    "---",
    "---",
    "---",
    "---",
    "---",
    "---",
    "---",
    "---",
    "---",
    "---",
    "---",
    "---",
};

enum ConfigRegisters
{
    ip,    // instruction pointer
    cid,   // core id
    ptbr,  // page table base register
    palim, // max possible physical address
};

enum Flags
{
    NF,
    ZF,
    CF,
    VF,
};

class Core
{
public:
    union InstructionData
    {
        struct Byte
        {
            uint8_t b3;
            uint8_t b2;
            uint8_t b1;
            uint8_t b0;
        };

        struct Hword
        {
            uint16_t h1;
            uint16_t h0;
        };

        Byte byte;
        Hword hword;
        uint32_t word;
    };

    Core(uint8_t id);

    void Start();

    Fault ExecuteInstruction();

    const uint32_t *const Registers() const { return this->registers; };
    const uint32_t *const ConfigRegisters() const { return this->configRegisters; };

private:
    uint32_t registers[16] = {0};
    uint32_t configRegisters[16] = {0};
    uint8_t Flags[4] = {0};
    uint64_t instructionCount = 0;

    Fault StackPush(uint8_t nBytes, uint32_t value);

    Fault StackPop(uint8_t nBytes, uint32_t &popTo);

    void JmpOp(uint32_t offset24Bit);

    void ArithmeticOp(uint8_t RD, uint32_t S1, uint32_t S2, uint8_t opCode);

    Fault ReadSizeFromAddress(uint8_t nBytes, uint32_t address, uint32_t &readTo);

    Fault WriteSizeToAddress(uint8_t nBytes, uint32_t address, uint32_t toWrite);

    Fault MovOp(InstructionData instruction, int nBytes);
};

#endif
