#include <string.h>
#include <stdio.h>
#include <iostream>
#include <fstream>

#include "memory.hpp"
#include "hardware.hpp"

uint8_t SystemMemory::ReadByte(uint32_t address)
{
    uint32_t pageAddr = PAGE_ALIGN(address);
    if(this->pages.count(pageAddr) == 0)
    {
        Page *p = new Page();
        memset(p->data, 0, PAGE_SIZE);
        this->pages[pageAddr] = p;
        this->activePages.insert(pageAddr >> PAGE_BIT_WIDTH);
    }
    // printf("Read from address %08x\n", address);
    return this->pages[pageAddr]->data[address & 0xfff];
}

void SystemMemory::WriteByte(uint32_t address, uint8_t value)
{
    uint32_t pageAddr = PAGE_ALIGN(address);
    if(this->pages.count(pageAddr) == 0)
    {
        Page *p = new Page();
        memset(p->data, 0, PAGE_SIZE);
        this->pages[pageAddr] = p;
        this->activePages.insert(pageAddr >> PAGE_BIT_WIDTH);
    }
    // printf("Write to address %08x\n", address);
    this->pages[pageAddr]->data[address & 0xfff] = value;
}

const std::set<uint32_t> &SystemMemory::ActivePages()
{
    return this->activePages;
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
    while (inFile.good())
    {
        inFile.read(&in, 1);
        writeB(i++, in);
    }
    inFile.close();
}

