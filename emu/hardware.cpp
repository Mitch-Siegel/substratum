#include "hardware.hpp"

#include "ui.hpp"

System hardware;

System::System()
{
    this->memory = SystemMemory(MEMORY_SIZE);

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
            wprintw(consoleWin, "EXCEPTION CAUGHT FROM CORE %d: ", i);
            switch (f)
            {
            case Fault::NO_FAULT:
                wprintw(consoleWin, "no fault\n");
                break;

            case Fault::PC_ALIGNMENT:
                wprintw(consoleWin, "program counter alignment fault\n");
                break;

            case Fault::INVALID_OPCODE:
                wprintw(consoleWin, "invalide opcode\n");
                break;

            case Fault::STACK_UNDERFLOW:
                wprintw(consoleWin, "stack underflow\n");
                break;

            case Fault::RETURN_STACK_CORRUPT:
                wprintw(consoleWin, "bp/sp corruption detected on ret instruction!\n");
                break;

            case Fault::PTB_PHYS:
                wprintw(consoleWin, "The physical address of the page table base exceeds the physical address space!\n");
                break;

            case Fault::INVALID_PAGE:
                wprintw(consoleWin, "Attempt to access a virtual address not mapped!\n");
                break;

            case Fault::PTD_INVALID:
                wprintw(consoleWin, "The physical address decoded from the page table exceeds the physical address space!\n");
                break;

            case Fault::HALTED:
                wprintw(consoleWin, "halted\n");
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
        wprintw(consoleWin, "%c", val);
        break;

    case 0x01:
        wprintw(consoleWin, "%u\n", val);
        break;

    case 0x02:
        wprintw(consoleWin, "%08x\n", val);
        break;

    default:
        wprintw(consoleWin, "Invalid port %d in output instruction!\n", port);
    }

    wrefresh(consoleWin);
}