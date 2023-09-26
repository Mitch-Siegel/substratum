#include "hardware.hpp"

System hardware;

System::System()
{
    for (uint8_t i = 0; i < CORE_COUNT; i++)
    {
        this->cores[i] = std::make_unique<Core>(i);
    }
}

void System::Start()
{
    this->running_ = true;
    for (uint8_t i = 0; i < CORE_COUNT; i++)
    {
        this->cores[i]->Start();
    }
}

bool System::Running()
{
    return this->running_;
}

void System::Tick()
{
    for (uint8_t i = 0; i < CORE_COUNT; i++)
    {
        Fault f = this->cores[i]->ExecuteInstruction();
        if (f != Fault::NO_FAULT)
        {
            printf("EXCEPTION CAUGHT FROM CORE %d: ", i);
            switch (f)
            {
            case Fault::NO_FAULT:
                printf("no fault\n");
                break;

            case Fault::PC_ALIGNMENT:
                printf("program counter alignment fault\n");
                break;

            case Fault::INVALID_OPCODE:
                printf("invalide opcode\n");
                break;

            case Fault::STACK_UNDERFLOW:
                printf("stack underflow\n");
                break;

            case Fault::RETURN_STACK_CORRUPT:
                printf("bp/sp corruption detected on ret instruction!\n");
                break;

            case Fault::HALTED:
                printf("halted\n");
                break;
            }
            this->running_ = false;
        }
    }
}

void System::Output(uint8_t port, uint32_t val)
{
    switch (port)
    {
    case 0x00:
        putchar(val);
        fflush(stdout);
        break;

    case 0x01:
        printf("%u\n", val);
        break;

    case 0x02:
        printf("%08x\n", val);
        break;

    default:
        printf("Invalid port %d in output instruction!\n", port);
    }
}