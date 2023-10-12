#include <string.h>
#include <stdio.h>
#include <iostream>
#include <fstream>

#include "memory.hpp"
#include "hardware.hpp"

#include "ui.hpp"

SystemMemory::SystemMemory()
{
    this->physicalMemory = nullptr;
    this->physicalMemorySize = 0;
}

SystemMemory::SystemMemory(uint32_t size)
{
    // this->physicalMemory = (uint8_t *)malloc(size);
    this->physicalMemory = new uint8_t[size];
    memset(this->physicalMemory, 0xbe, size);
    printf("Allocated %u bytes (%p) for vm memory\n", size, this->physicalMemory);
    this->physicalMemorySize = size;
}

SystemMemory::~SystemMemory()
{
    if (this->physicalMemory != nullptr)
    {
        delete[] this->physicalMemory;
        this->physicalMemory = nullptr;
    }
    this->physicalMemorySize = 0;
}

#define PTE_EXISTS(address) (address & 0b1)
#define L1MASK(address) ((((uint32_t)address) >> (12)) & 0x3FF)
#define L2MASK(address) ((((uint32_t)address) >> (12 + 10)) & 0x3FF)
#define PTE_TO_PHYS(pte) (((pte) >> 10) << 12)

Fault SystemMemory::WalkPageTable(uint32_t ptba, uint32_t va, uint32_t &paTo)
{
    if (ptba > this->physicalMemorySize)
    {
        return Fault::PTB_PHYS;
    }

    if (ptba == 0)
    {
        if (va >= this->physicalMemorySize)
        {
            return Fault::PTB_PHYS;
        }
        paTo = va;
        return Fault::NO_FAULT;
    }

    // physical memory as page table entries
    uint32_t *pt = reinterpret_cast<uint32_t *>(this->physicalMemory);

    // walk the first level of the table
    uint32_t *pte = &pt[L1MASK(va)];
    if (!PTE_EXISTS(*pte))
    {
        return Fault::INVALID_PAGE;
    }
    pt = (uint32_t *)PTE_TO_PHYS(*pte);

    pte = &pt[L2MASK(va)];
    if (!PTE_EXISTS(*pte))
    {
        return Fault::INVALID_PAGE;
    }
    pt = (uint32_t *)PTE_TO_PHYS(*pte);

    paTo = *pt;
    if (paTo >= this->physicalMemorySize)
    {
        return Fault::PTD_INVALID;
    }

    return Fault::NO_FAULT;
}

Fault SystemMemory::ReadByte(uint32_t ptba, uint32_t address, uint32_t &readTo)
{
    uint32_t pa = 0;
    Fault f = this->WalkPageTable(ptba, address, pa);
    if (f != Fault::NO_FAULT)
    {
        return f;
    }
    readTo = this->physicalMemory[pa];

    return Fault::NO_FAULT;
}

Fault SystemMemory::WriteByte(uint32_t ptba, uint32_t address, uint32_t value)
{
    uint32_t pa = 0;
    if (ptba > this->physicalMemorySize)
    {
        return Fault::PTB_PHYS;
    }
    Fault f = this->WalkPageTable(ptba, address, pa);
    if (f != Fault::NO_FAULT)
    {
        return f;
    }

    // directly intercept uart memory status changes
    // if the UART is writing "to us", print it out to the screen
    // otherwise it is clearing its receive register indicating it's ready to receive again
    else if(pa == (MEMMAP_UART))
    {
        if(value != 8)
        {
            ui.wprintw_threadsafe(consoleWin, "%c", value);
        }
        else
        {
            int y;
            int x;
            getyx(consoleWin, y, x);
            ui.mvwprintw_threadsafe(consoleWin, y, x - 1, " ");
            ui.mvwprintw_threadsafe(consoleWin, y, x - 1, "");
        }
        this->physicalMemory[MEMMAP_UART + 1] = 0x00;
    }
    else
    {
        this->physicalMemory[pa] = value & 0xff;
    }

    return Fault::NO_FAULT;
}

Fault SystemMemory::ReadHalfWord(uint32_t ptba, uint32_t address, uint32_t &readTo)
{
    uint32_t pa = 0;
    Fault f = this->WalkPageTable(ptba, address, pa);
    if (f != Fault::NO_FAULT)
    {
        return f;
    }

    readTo = this->physicalMemory[pa + 1];
    readTo |= this->physicalMemory[pa] << 8;

    return Fault::NO_FAULT;
}

Fault SystemMemory::WriteHalfWord(uint32_t ptba, uint32_t address, uint32_t value)
{
    uint32_t pa = 0;
    if (ptba > this->physicalMemorySize)
    {
        return Fault::PTB_PHYS;
    }
    Fault f = this->WalkPageTable(ptba, address, pa);
    if (f != Fault::NO_FAULT)
    {
        return f;
    }

    this->physicalMemory[pa + 1] = value & 0xff;
    this->physicalMemory[pa] = (value >> 8) & 0xff;

    return Fault::NO_FAULT;
}

Fault SystemMemory::ReadWord(uint32_t ptba, uint32_t address, uint32_t &readTo)
{
    uint32_t pa = 0;
    Fault f = this->WalkPageTable(ptba, address, pa);
    if (f != Fault::NO_FAULT)
    {
        return f;
    }

    readTo = this->physicalMemory[pa + 3];
    readTo |= this->physicalMemory[pa + 2] << 8;
    readTo |= this->physicalMemory[pa + 1] << 16;
    readTo |= this->physicalMemory[pa] << 24;

    return Fault::NO_FAULT;
}

Fault SystemMemory::WriteWord(uint32_t ptba, uint32_t address, uint32_t value)
{
    uint32_t pa = 0;
    if (ptba > this->physicalMemorySize)
    {
        return Fault::PTB_PHYS;
    }
    Fault f = this->WalkPageTable(ptba, address, pa);
    if (f != Fault::NO_FAULT)
    {
        return f;
    }

    this->physicalMemory[pa + 3] = value & 0xff;
    this->physicalMemory[pa + 2] = (value >> 8) & 0xff;
    this->physicalMemory[pa + 1] = (value >> 16) & 0xff;
    this->physicalMemory[pa] = (value >> 24) & 0xff;

    return Fault::NO_FAULT;
}

void SystemMemory::InitializeFromFile(char *filePath)
{
    std::ifstream inFile;
    inFile.open(filePath, std::ifstream::in);
    if (!inFile.good())
    {
        std::cout << "Please enter valid bin file" << std::endl;
        exit(1);
    }
    int i = 0;
    char in;
    printf("\n%04x: ", i);
    while (inFile.good())
    {
        inFile.read(&in, 1);
        this->physicalMemory[i] = in & 0xff;
        printf("%02x ", in & 0xff);
        if ((i + 1) % 16 == 0)
        {
            printf("\n%04x: ", i + 1);
        }

        i++;
    }
    printf("\n");
    inFile.close();

    printf("Loaded physical memory image (%d bytes)\n", i + 1);
}
