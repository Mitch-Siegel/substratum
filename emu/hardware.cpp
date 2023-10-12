#include "hardware.hpp"

#include "ui.hpp"

System hardware;

System::System()
{
    this->memory = std::make_unique<SystemMemory>(MEMORY_SIZE);

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

void System::Stop()
{
    this->running_ = false;
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
            ui.wprintw_threadsafe(consoleWin, "\nEXCEPTION CAUGHT FROM CORE %d: ", i);
            switch (f)
            {
            case Fault::NO_FAULT:
                ui.wprintw_threadsafe(consoleWin, "no fault\n");
                break;

            case Fault::PC_ALIGNMENT:
                ui.wprintw_threadsafe(consoleWin, "program counter alignment fault\n");
                break;

            case Fault::INVALID_OPCODE:
                ui.wprintw_threadsafe(consoleWin, "invalide opcode\n");
                break;

            case Fault::STACK_UNDERFLOW:
                ui.wprintw_threadsafe(consoleWin, "stack underflow\n");
                break;

            case Fault::RETURN_STACK_CORRUPT:
                ui.wprintw_threadsafe(consoleWin, "bp/sp corruption detected on ret instruction!\n");
                break;

            case Fault::PTB_PHYS:
                ui.wprintw_threadsafe(consoleWin, "The physical address of the page table base exceeds the physical address space!\n");
                break;

            case Fault::INVALID_PAGE:
                ui.wprintw_threadsafe(consoleWin, "Attempt to access a virtual address not mapped!\n");
                break;

            case Fault::PTD_INVALID:
                ui.wprintw_threadsafe(consoleWin, "The physical address decoded from the page table exceeds the physical address space!\n");
                break;

            case Fault::RO_WRITE:
                ui.wprintw_threadsafe(consoleWin, "Attempt to write a physical address that is read-only!\n");
                break;

            case Fault::ILLEGAL_CSR_WRITE:
                ui.wprintw_threadsafe(consoleWin, "Illegal csr write!\n");
                break;

            case Fault::ILLEGAL_CSR_READ:
                ui.wprintw_threadsafe(consoleWin, "Illegal read write!\n");
                break;

            case Fault::HALTED:
                ui.wprintw_threadsafe(consoleWin, "halted\n");
                break;
            }
            wrefresh(consoleWin);
            this->running_ = false;
        }
    }
}

void System::Interrupt(uint8_t index)
{
    this->cores[0]->Interrupt(index);
}

void System::Output(uint8_t port, uint32_t val)
{
    switch (port)
    {
    case 0x00:
        ui.wprintw_threadsafe(consoleWin, "%c", val);
        break;

    case 0x01:
        ui.wprintw_threadsafe(consoleWin, "%u\n", val);
        break;

    case 0x02:
        ui.wprintw_threadsafe(consoleWin, "%08x\n", val);
        break;

    default:
        ui.wprintw_threadsafe(consoleWin, "Invalid port %d in output instruction!\n", port);
    }

}