#include <cstdint>
#include <string>
#include <type_traits>
#include <semaphore.h>
#include <cassert>

#include "memory.hpp"
#include "faults.hpp"
#include "ui.hpp"


#ifndef _CORE_HPP_
#define _CORE_HPP_

const static std::string registerNames[16] = {
    "%r0", "%r1", "%r2", "%r3", "%r4", "%r5", "%r6", "%r7", "%r8", "%r9", "%ra", "%rb", "%rc", "%rr", "%sp", "%bp"};

enum class Registers
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
    "mapb",
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

enum class ConfigRegisters
{
    ip,    // instruction pointer
    cid,   // core id
    ptbr,  // page table base register
    palim, // max possible physical address
    mapb,  // base of reserved section of memory map
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
    sem_t *sem;

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

    ~Core();

    void Start();

    Fault ExecuteInstruction();

    void Interrupt(uint8_t index);

    bool Interrupted();

    const uint32_t *const Registers() const { return this->registers.data; };
    const uint32_t *const ConfigRegisters() const { return this->configRegisters.data; };

private:
    class
    {
    public:
        uint32_t &operator[](enum Registers index)
        {
            return this->data[static_cast<std::underlying_type_t<enum Registers>>(index)];
        };

        uint32_t &operator[](uint8_t index)
        {
            assert(index < 16);
            return this->data[index];
        };

        uint32_t data[16] = {0};
    } registers;

    class
    {
    public:
        uint32_t &operator[](enum ConfigRegisters index)
        {
            return this->data[static_cast<std::underlying_type_t<enum ConfigRegisters>>(index)];
        };

        uint32_t data[16] = {0};
    } configRegisters;

    uint8_t Flags[4] = {0};
    uint64_t instructionCount = 0;

    struct
    {
        uint32_t registers[16];
        uint32_t configRegisters[16];
        uint8_t flags[4];
    } interruptContext;
    pthread_mutex_t interruptLock;

    Fault WriteCSR(const enum ConfigRegisters CSRRD, const uint8_t RS);

    Fault ReadCSR(const uint8_t RD, const enum ConfigRegisters CSRRS);

    Fault StackPush(uint8_t nBytes, uint32_t value);

    Fault StackPop(uint8_t nBytes, uint32_t &popTo);

    void JmpOp(uint32_t offset24Bit);

    void ArithmeticOp(uint8_t RD, uint32_t S1, uint32_t S2, uint8_t opCode);

    Fault ReadSizeFromAddress(uint8_t nBytes, uint32_t address, uint32_t &readTo);

    Fault WriteSizeToAddress(uint8_t nBytes, uint32_t address, uint32_t toWrite);

    Fault MovOp(InstructionData instruction, int nBytes);

    void InterruptReturn();
};

#endif
