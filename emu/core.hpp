#include <cstdint>
#include <string>

#include "memory.hpp"

#ifndef _CORE_HPP_
#define _CORE_HPP_

const static std::string registerNames[17] = {
    "%r0", "%r1", "%r2", "%r3", "%r4", "%r5", "%r6", "%r7", "%r8", "%r9", "%ra", "%rb", "%rc", "%rr", "%%sp", "%bp", "%%ip"};

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

enum ConfigRegisters
{
    ip,  // instruction pointer
    cid, // core id
};

enum Flags
{
    NF,
    ZF,
    CF,
    VF,
};

enum class Fault
{
    NO_FAULT,
    PC_ALIGNMENT,
    INVALID_OPCODE,
    STACK_UNDERFLOW,
    RETURN_STACK_CORRUPT,
    HALTED,
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

private:
    int32_t registers[16] = {0};
    int32_t configRegisters[16] = {0};
    uint8_t Flags[4] = {0};
    uint64_t instructionCount = 0;

    void StackPush(uint32_t value, uint8_t nBytes);

    uint32_t StackPop(uint8_t nBytes);

    void JmpOp(uint32_t offset24Bit);

    void ArithmeticOp(uint8_t RD, uint32_t S1, uint32_t S2, uint8_t opCode);

    void MovOp(InstructionData instruction, int nBytes);
};

#endif
