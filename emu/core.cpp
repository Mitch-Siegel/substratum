#include "core.hpp"

#include "hardware.hpp"
#include "names.hpp"

#include <ncurses.h>

Core::Core(uint8_t id)
{
    this->configRegisters[cid] = id;
}

void Core::Start()
{
    this->registers[Registers::sp] = 0xfffffffc;
    this->configRegisters[ConfigRegisters::ip] = readW(0); // read the entry point to the code segment

    printw("Read entry address of %08x\n", configRegisters[ConfigRegisters::ip]);
}

void Core::StackPush(uint32_t value, uint8_t nBytes)
{
    switch (nBytes)
    {
    case 1:
        this->registers[Registers::sp] -= 1;
        writeB(this->registers[Registers::sp], value);
        break;

    case 2:
        this->registers[Registers::sp] -= 2;
        writeH(this->registers[Registers::sp], value);
        break;

    case 4:
        this->registers[Registers::sp] -= 4;
        writeW(this->registers[Registers::sp], value);
        break;

    default:
        printf("Illegal argument to stackPush!");
        exit(1);
    }
}

uint32_t Core::StackPop(uint8_t nBytes)
{
    uint32_t value;
    switch (nBytes)
    {
    case 1:
        value = readB(this->registers[Registers::sp]);
        this->registers[Registers::sp] += 1;
        break;

    case 2:
        value = readH(this->registers[Registers::sp]);
        this->registers[Registers::sp] += 2;
        break;

    case 4:
        value = readW(this->registers[Registers::sp]);
        this->registers[Registers::sp] += 4;
        break;

    default:
        printf("Illegal argument to StackPop!");
        exit(1);
    }

    return value;
}

void Core::JmpOp(uint32_t offset24Bit)
{
    static const uint64_t mask = 1U << (23);
    uint32_t relativeAddress = ((offset24Bit ^ mask) - mask) << 2;
    // printf("Jmp by %d (raw offset was %u)\n", relativeAddress, offset24Bit);
    this->configRegisters[ConfigRegisters::ip] += relativeAddress;
}

void Core::ArithmeticOp(uint8_t RD, uint32_t S1, uint32_t S2, uint8_t opCode)
{
    // printf("Arithmetic op with %d %d\n", S1, S2);
    uint64_t result = 0UL;

    // their compiler optimizes better than mine
    uint32_t invertedS2 = ~S2;
    switch (opCode & 0b11111)
    {
    case 0x1: // ADD
        result = S1 + S2;
        Flags[CF] = (result < S1);
        break;
    case 0x3: // SUB
        result = S1 + invertedS2 + 1;
        Flags[CF] = !(result > S1);
        break;
    case 0x5: // MUL
        result = S1 * S2;
        break;
    case 0x7: // DIV
        result = S1 / S2;
        break;
    case 0x9: // SHR
        result = S1 >> S2;
        break;
    case 0xb: // SHL
        result = S1 << S2;
        break;
    case 0xd: // AND
        result = S1 & S2;
        break;
    case 0xf: // OR
        result = S1 | S2;
        break;
    case 0x11: // XOR
        result = S1 ^ S2;
        break;
    case 0x13: // CMP
        // uint16_t result = this->registers[RD] - value;
        result = S1 + invertedS2 + 1;
        Flags[CF] = !(result > S1);
        break;
    }

    Flags[ZF] = ((result & 0xffffffff) == 0);
    Flags[NF] = ((result >> 31) & 1);
    // Flags[CF] = ((result >> 32) > 0);

    // this seems wrong... is it?
    Flags[VF] = ((this->registers[RD] >> 31) ^ ((result >> 31) & 1)) && !(1 ^ (this->registers[RD] >> 31) ^ (S2 >> 31));

    // printf("NF: %d ZF: %d CF: %d VF: %d \n", Flags[NF], Flags[ZF], Flags[CF], Flags[VF]);
    // if the two inputs have MSB set
    /*if (this->registers[RD] >> 15 && value >> 15)
    {
        // opposite of whether result has sign flag
        Flags[VF] = !((result >> 15) > 0);
    }
    else
    {
        if (!(this->registers[RD] >> 15) && !(value >> 15))
        {
            Flags[VF] = (result >> 15) > 0;
        }
        else
        {
            Flags[VF] = 0;
        }
    }*/

    // set result if not CMP
    if ((opCode & 0b11111) != 0x13)
    {
        // if (RD != 0)
        this->registers[RD] = result;
    }
}

void Core::MovOp(InstructionData instruction, int nBytes)
{

    switch (instruction.byte.b0 & 0b1111)
    {
        // mov reg, reg
    case 0x0:
    {
        uint8_t RS = instruction.byte.b1 >> 4;
        uint8_t RD = instruction.byte.b1 & 0b1111;
        uint32_t value;

#ifdef PRINTEXECUTION
        printf("%%r%d, %%r%d\n", RD, RS);
#endif

        switch (nBytes)
        {
        case 1:
            value = this->registers[RS] & 0x000000ff;
            break;
        case 2:
            value = this->registers[RS] & 0x0000ffff;
            break;
        case 4:
            value = this->registers[RS] & 0xffffffff;
            break;
        default:
            printf("Illegal nBytes argument to MovOp!\n");
            exit(1);
        };
        this->registers[RD] = value;
    }
    break;

    // mov reg (reg)
    case 0x2:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
        uint8_t rBase = instruction.byte.b1 & 0b1111;

#ifdef PRINTEXECUTION
        printf("%%r%d, (%%r%d)\t(%08x)\n", RD, rBase, this->registers[rBase]);
#endif

        switch (nBytes)
        {
        case 1:
            // this->registers[RD] &= 0xffffff00;
            this->registers[RD] = readB(this->registers[rBase]);
            break;
        case 2:
            // this->registers[RD] &= 0xffff0000;
            this->registers[RD] = readH(this->registers[rBase]);
            break;
        case 4:
            // this->registers[RD] &= 0x00000000;
            this->registers[RD] = readW(this->registers[rBase]);
            break;
        default:
            printf("Illegal nBytes argument to MovOp!\n");
            exit(1);
        };
    }
    break;

    // mov (reg), reg
    case 0x4:
    {
        uint8_t rBase = instruction.byte.b1 >> 4;
        uint8_t RS = instruction.byte.b1 & 0b1111;

#ifdef PRINTEXECUTION
        printf("(%%r%d), %%r%d\t(%08x)\n", rBase, RS, this->registers[rBase]);
#endif

        switch (nBytes)
        {
        case 1:
            writeB(this->registers[rBase], this->registers[RS]);
            break;
        case 2:
            writeH(this->registers[rBase], this->registers[RS]);
            break;
        case 4:
            writeW(this->registers[rBase], this->registers[RS]);
            break;
        default:
            printf("Illegal nBytes argument to MovOp!\n");
            exit(1);
        };
    }
    break;

    // mov reg, (reg +/- offimm)
    case 0x5:
    case 0x6:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
        uint8_t rBase = instruction.byte.b1 & 0b1111;
        int16_t offset = instruction.hword.h1;

        int64_t longAddress = this->registers[rBase];

        if ((instruction.byte.b0 & 0b1111) == 0x5)
        {
            longAddress += offset;
        }
        else if ((instruction.byte.b0 & 0b1111) == 0x6)
        {
            longAddress -= offset;
        }
        else
        {
            printf("Error decoding instruction with opcode %02x\n", instruction.byte.b1);
        }

        uint32_t address = longAddress;

#ifdef PRINTEXECUTION
        printf("%%r%d, (%%r%d+%d)\t(%08x)\n", RD, rBase, offset, address);
#endif

        // printf("address %08x + %d = %08x\n", this->registers[rBase], offset, address);

        switch (nBytes)
        {
        case 1:
            this->registers[RD] = readB(address);
            break;
        case 2:
            this->registers[RD] = readH(address);
            break;
        case 4:
            this->registers[RD] = readW(address);
            break;
        default:
            printf("Illegal nBytes argument to MovOp!\n");
            exit(1);
        };
    }
    break;

    // mov (reg +/- offimm), reg
    case 0x7:
    case 0x8:
    {
        uint8_t RS = instruction.byte.b1 >> 4;
        uint8_t rBase = instruction.byte.b1 & 0b1111;
        int16_t offset = instruction.hword.h1;

        int64_t longAddress = this->registers[rBase];

        if ((instruction.byte.b0 & 0b1111) == 0x7)
        {
            longAddress += offset;
        }
        else if ((instruction.byte.b0 & 0b1111) == 0x8)
        {
            longAddress -= offset;
        }
        else
        {
            printf("Error decoding instruction with opcode %02x\n", instruction.byte.b1);
        }

        int32_t address = longAddress;

#ifdef PRINTEXECUTION
        printf("(%%r%d + %d), %%r%d\t(%08x)\n", rBase, offset, RS, address);
#endif

        // printf("address %08x + %d = %08x\n", this->registers[rBase], offset, address);

        switch (nBytes)
        {
        case 1:
            writeB(address, this->registers[RS]);
            break;
        case 2:
            writeH(address, this->registers[RS]);
            break;
        case 4:
            writeW(address, this->registers[RS]);
            break;
        default:
            printf("Illegal nBytes argument to MovOp!\n");
            exit(1);
        };
    }
    break;

    // mov reg, (reg + offreg * sclpow)
    case 0x9:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
        uint8_t rBase = instruction.byte.b1 & 0b1111;

        uint8_t rOff = instruction.byte.b2 & 0b1111;
        uint8_t sclPow = instruction.byte.b3 & 0b11111;
        uint32_t offset = this->registers[rOff];

        int64_t longaddress = this->registers[rBase];
        longaddress += (offset << sclPow);
        uint32_t address = longaddress;

#ifdef PRINTEXECUTION
        printf("%%r%d, (%%r%d+%%r%d*%d)\t(%08x)\n", RD, rBase, rOff, (1 << sclPow), address);
#endif

        switch (nBytes)
        {
        case 1:
            this->registers[RD] = readB(address);
            break;
        case 2:
            this->registers[RD] = readH(address);
            break;
        case 4:
            this->registers[RD] = readW(address);
            break;
        default:
            printf("Illegal nBytes argument to MovOp!\n");
            exit(1);
        };
    }
    break;

    // mov (reg + offreg * sclpow), reg
    case 0xb:
    {
        uint8_t RS = instruction.byte.b1 >> 4;
        uint8_t rBase = instruction.byte.b1 & 0b1111;

        uint8_t rOff = instruction.byte.b2 & 0b1111;
        uint8_t sclPow = instruction.byte.b3 & 0b11111;
        uint32_t offset = this->registers[rOff];

        int64_t longaddress = this->registers[rBase];
        longaddress += (offset << sclPow);
        uint32_t address = longaddress;

#ifdef PRINTEXECUTION
        printf("(%%r%d+%%r%d*%d), %%r%d\t(%08x)<-", rBase, rOff, (1 << sclPow), RS, address);
        switch (nBytes)
        {
        case 1:
            printf("%02x (%d/%u)\n", this->registers[RS], this->registers[RS], this->registers[RS]);
            break;
        case 2:
            printf("%04x (%d/%u)\n", this->registers[RS], this->registers[RS], this->registers[RS]);
            break;
        case 4:
            printf("%08x (%d/%u)\n", this->registers[RS], this->registers[RS], this->registers[RS]);
            break;
        default:
            break;
        }
#endif

        switch (nBytes)
        {
        case 1:
            writeB(address, this->registers[RS]);
            break;
        case 2:
            writeH(address, this->registers[RS]);
            break;
        case 4:
            writeW(address, this->registers[RS]);
            break;
        default:
            printf("Illegal nBytes argument to MovOp!\n");
            exit(1);
        };
    }
    break;

    // reg, imm
    case 0xf:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
#ifdef PRINTEXECUTION
        printf("%%r%d, %d\n", RD, instruction.hword.h1);
#endif

        switch (nBytes)
        {
        case 1:
            this->registers[RD] = instruction.byte.b3;
            break;
        case 2:
            this->registers[RD] = instruction.hword.h1;
            break;
        case 4:
        default:
            printf("Illegal nBytes argument to MovOp!\n");
            exit(1);
        };
    }
    break;

    default:
        printf("Illegal MovOp instruction!\n");
        exit(1);
    }
}

Fault Core::ExecuteInstruction()
{
    InstructionData instruction = {{0}};
    if (this->configRegisters[ConfigRegisters::ip] & (0b11))
    {
        return Fault::PC_ALIGNMENT;
    }

    instruction.word = consumeW(this->configRegisters[ConfigRegisters::ip]);
    if (opcodeNames.count(instruction.byte.b0) == 0)
    {
        return Fault::INVALID_OPCODE;
    }
    else
    {
#ifdef PRINTEXECUTION
        std::string opcodeName = opcodeNames[instruction.byte.b0];
        printf("%08x: %02x:%-4s ", this->configRegisters[ConfigRegisters::ip] - 4, instruction.byte.b0, opcodeName.substr(0, opcodeName.find(" ")).c_str());
#endif
    }

    // printf("%02x, %02x, %02x, %02x | %04x, %04x | %08x\n", instruction.byte.b0, instruction.byte.b1, instruction.byte.b2, instruction.byte.b3, instruction.hword.h0, instruction.hword.h1, instruction.word);

    switch (instruction.byte.b0)
    {
        // hlt instruction
    case 0x00:
    {
#ifdef PRINTEXECUTION
        printf("\n");
#endif
        return Fault::HALTED;
    }
    break;

    case 0x01: // nop
    {
#ifdef PRINTEXECUTION
        printf("\n");
#endif
    }
    break;

    // JMP
    case 0x11:
    {
#ifdef PRINTEXECUTION
        printf("\n");
#endif
        // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", Flags[NF], Flags[ZF], Flags[CF], Flags[VF]);
        this->JmpOp(instruction.word & 0x00ffffff);
    }
    break;

    // JE/JZ
    case 0x13:
    {
        // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", Flags[NF], Flags[ZF], Flags[CF], Flags[VF]);

        if (Flags[ZF])
        {
#ifdef PRINTEXECUTION
            printf("TAKEN\n");
#endif
            this->JmpOp(instruction.word & 0x00ffffff);
        }
#ifdef PRINTEXECUTION
        else
        {
            printf("NOT TAKEN\n");
        }
#endif
    }
    break;

    // JNE/JNZ
    case 0x15:
    {
        // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", Flags[NF], Flags[ZF], Flags[CF], Flags[VF]);

        if (!Flags[ZF])
        {
#ifdef PRINTEXECUTION
            printf("TAKEN\n");
#endif
            this->JmpOp(instruction.word & 0x00ffffff);
        }
#ifdef PRINTEXECUTION
        else
        {
            printf("NOT TAKEN\n");
        }
#endif
    }
    break;

    // JG
    case 0x17:
    {
        // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", Flags[NF], Flags[ZF], Flags[CF], Flags[VF]);

        if ((!Flags[ZF]) && Flags[CF])
        {
#ifdef PRINTEXECUTION
            printf("TAKEN\n");
#endif
            this->JmpOp(instruction.word & 0x00ffffff);
        }
#ifdef PRINTEXECUTION
        else
        {
            printf("NOT TAKEN\n");
        }
#endif
    }
    break;

    // JL
    case 0x19:
    {
        // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", Flags[NF], Flags[ZF], Flags[CF], Flags[VF]);

        if (!Flags[CF])
        {
#ifdef PRINTEXECUTION
            printf("TAKEN\n");
#endif
            this->JmpOp(instruction.word & 0x00ffffff);
        }
#ifdef PRINTEXECUTION
        else
        {
            printf("NOT TAKEN\n");
        }
#endif
    }
    break;

    // JGE
    case 0x1b:
    {
        // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", Flags[NF], Flags[ZF], Flags[CF], Flags[VF]);

        if (Flags[CF])
        {
#ifdef PRINTEXECUTION
            printf("TAKEN\n");
#endif
            this->JmpOp(instruction.word & 0x00ffffff);
        }
#ifdef PRINTEXECUTION
        else
        {
            printf("NOT TAKEN\n");
        }
#endif
    }
    break;

    // JLE
    case 0x1d:
    {
        // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", Flags[NF], Flags[ZF], Flags[CF], Flags[VF]);

        if (Flags[ZF] || (!Flags[CF]))
        {
#ifdef PRINTEXECUTION
            printf("TAKEN\n");
#endif
            this->JmpOp(instruction.word & 0x00ffffff);
        }
#ifdef PRINTEXECUTION
        else
        {
            printf("NOT TAKEN\n");
        }
#endif
    }
    break;

    // unsigned arithmetic
    case 0x41:
    case 0x43:
    case 0x45:
    case 0x47:
    case 0x49:
    case 0x4b:
    case 0x4d:
    case 0x4f:
    case 0x51:
    case 0x53: // simple arithmetic
    {
        uint8_t RD = instruction.byte.b1 >> 4;
        uint8_t RS1 = instruction.byte.b1 & 0b1111;
        uint8_t RS2 = instruction.byte.b2 >> 4;
#ifdef PRINTEXECUTION
        printf("%%r%d, %%r%d, %%r%d\n", RD, RS1, RS2);
#endif
        this->ArithmeticOp(RD, this->registers[RS1], this->registers[RS2], instruction.byte.b0);
    }
    break;

    // INC
    case 0x54:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
#ifdef PRINTEXECUTION
        printf("%%r%d\n", RD);
#endif
        this->registers[RD]++;
    }
    break;

    // DEC
    case 0x56:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
#ifdef PRINTEXECUTION
        printf("%%r%d\n", RD);
#endif
        this->registers[RD]--;
    }
    break;

    // NOT
    case 0x58:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
        uint8_t RS1 = instruction.byte.b1 & 0b1111;
#ifdef PRINTEXECUTION
        printf("%%r%d, %%r%d\n", RD, RS1);
#endif
        this->registers[RD] = ~this->registers[RS1];
    }
    break;

    case 0x61:
    case 0x63:
    case 0x65:
    case 0x67:
    case 0x69:
    case 0x6b:
    case 0x6d:
    case 0x6f:
    case 0x71:
    case 0x73:
        // immediate arithmetic
        {
            uint8_t RD = instruction.byte.b1 >> 4;
            uint8_t RS1 = instruction.byte.b1 & 0b1111;
            uint16_t imm = instruction.hword.h1;
#ifdef PRINTEXECUTION
            printf("%%r%d, %%r%d, %u\n", RD, RS1, imm);
#endif
            this->ArithmeticOp(RD, this->registers[RS1], imm, instruction.byte.b0);
        }
        break;

    // movb
    case 0xa0:
    case 0xa2:
    case 0xa4:
    case 0xa5:
    case 0xa6:
    case 0xa7:
    case 0xa8:
    case 0xa9:
    case 0xab:
    case 0xaf:
    {
        this->MovOp(instruction, 1);
    }
    break;

    // movh
    case 0xb0:
    case 0xb2:
    case 0xb4:
    case 0xb5:
    case 0xb6:
    case 0xb7:
    case 0xb8:
    case 0xb9:
    case 0xbb:
    case 0xbf:
    {
        this->MovOp(instruction, 2);
    }
    break;

    // mov
    case 0xc0:
    case 0xc2:
    case 0xc4:
    case 0xc5:
    case 0xc6:
    case 0xc7:
    case 0xc8:
    case 0xc9:
    case 0xcb:
    {
        this->MovOp(instruction, 4);
    }
    break;

    case 0xcc:
        // LEA - addressing mode 2 (base + offset)
        {
            uint8_t RD = instruction.byte.b1 >> 4;
            uint8_t rBase = instruction.byte.b1 & 0b1111;
            int16_t offset = instruction.hword.h1;

            int64_t longAddress = this->registers[rBase];

            longAddress += offset;

            uint32_t address = longAddress;

#ifdef PRINTEXECUTION
            printf("%%r%d, (%%r%d+%d)\t(%08x)\n", RD, rBase, offset, address);
#endif

            this->registers[RD] = address;
        }
        break;

    case 0xcd:
        // LEA - addressing mode 3 (base + offset*sclpow)
        {
            uint8_t RD = instruction.byte.b1 >> 4;
            uint8_t rBase = instruction.byte.b1 & 0b1111;

            uint8_t rOff = instruction.byte.b2 & 0b1111;
            uint8_t sclPow = instruction.byte.b3 & 0b11111;
            uint32_t offset = this->registers[rOff];

            int64_t longaddress = this->registers[rBase];
            longaddress += (offset << sclPow);
            uint32_t address = longaddress;

#ifdef PRINTEXECUTION
            printf("%%r%d, (%%r%d+%%r%d*%d)\t(%08x)\n", RD, rBase, rOff, (1 << sclPow), address);
#endif
            this->registers[RD] = address;
        }
        break;

    // stack, call/return

    // PUSHB
    case 0xd0:
    {
        uint8_t RS = instruction.byte.b1 & 0b1111;
#ifdef PRINTEXECUTION
        printf("%%r%d\n", RS);
#endif
        this->StackPush(this->registers[RS], 1);
    }
    break;

    // PUSHH
    case 0xd1:
    {
        uint8_t RS = instruction.byte.b1 & 0b1111;
#ifdef PRINTEXECUTION
        printf("%%r%d\n", RS);
#endif
        this->StackPush(this->registers[RS], 2);
    }
    break;

    // PUSH
    case 0xd2:
    {
        uint8_t RS = instruction.byte.b1 & 0b1111;
#ifdef PRINTEXECUTION
        printf("%%r%d\n", RS);
#endif
        this->StackPush(this->registers[RS], 4);
    }
    break;

    // POPB
    case 0xd3:
    {
        uint8_t RD = instruction.byte.b1 & 0b1111;
#ifdef PRINTEXECUTION
        printf("%%r%u\n", RD);
#endif
        // fault condition by which attempt to pop from a completely empty stack
        if (this->registers[Registers::sp] >= 0xfffffffc)
        {
            return Fault::STACK_UNDERFLOW;
        }

        this->registers[RD] = this->StackPop(1);
    }
    break;

    // POPH
    case 0xd4:
    {
        uint8_t RD = instruction.byte.b1 & 0b1111;
#ifdef PRINTEXECUTION
        printf("%%r%u\n", RD);
#endif
        // fault condition by which attempt to pop from a completely empty stack
        if (this->registers[Registers::sp] >= 0xfffffffc)
        {
            return Fault::STACK_UNDERFLOW;
        }

        this->registers[RD] = this->StackPop(2);
    }
    break;

    // POP
    case 0xd5:
    {
        uint8_t RD = instruction.byte.b1 & 0b1111;
#ifdef PRINTEXECUTION
        printf("%%r%u\n", RD);
#endif
        // fault condition by which attempt to pop from a completely empty stack
        if (this->registers[Registers::sp] >= 0xfffffffc)
        {
            return Fault::STACK_UNDERFLOW;
        }

        this->registers[RD] = this->StackPop(4);
    }
    break;

    // CALL
    case 0xd6:
    {
        uint32_t callAddr = instruction.word << 8;
#ifdef PRINTEXECUTION
        printf("%08x\n", callAddr);
#endif
        // printf("call to %08x\n", callAddr);
        StackPush(this->registers[Registers::bp], 4);
        StackPush(this->configRegisters[ConfigRegisters::ip], 4);
        this->registers[Registers::bp] = this->registers[Registers::sp];
        this->configRegisters[ConfigRegisters::ip] = callAddr;
    }
    break;

    // RET
    case 0xd7:
    {
        uint32_t argw = instruction.word & 0x00ffffff;
#ifdef PRINTEXECUTION
        printf("%d\n", argw);
#endif
        if (this->registers[Registers::sp] != this->registers[Registers::bp])
        {
            return Fault::RETURN_STACK_CORRUPT;
        }
        this->configRegisters[ConfigRegisters::ip] = StackPop(4);
        this->registers[Registers::bp] = StackPop(4);
        this->registers[Registers::sp] += argw;
    }
    break;

    // OUT
    case 0xe2:
    {
        uint8_t port = instruction.byte.b1;
        uint8_t rs = instruction.byte.b2 & 0xf;
        hardware.Output(port, this->registers[rs]);
        // printf("\t\t%%r%d: %u\n", rs, this->registers[rs]);
    }
    break;

    case 0xfe: // HLT
    {
#ifdef PRINTEXECUTION
        printf("\n");
#endif
        return Fault::HALTED;
    }
    break;

    default:
        return Fault::INVALID_OPCODE;
        break;
    }

    // printState();
    // printf("\n");

    // {
    // using namespace std::chrono_literals;
    // std::this_thread::sleep_for(20ms);
    // }

    // printf("\n");

    this->instructionCount++;
    return Fault::NO_FAULT;
}
