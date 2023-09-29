
#include <cstdint>
#include <map>
#include <set>

#include "faults.hpp"

#ifndef _MEMORY_HPP_
#define _MEMORY_HPP_

#define readB(ptb, address, dest) hardware.memory.ReadByte(ptb, address, dest);
#define readH(ptb, address, dest) hardware.memory.ReadHalfWord(ptb, address, dest)
#define readW(ptb, address, dest) hardware.memory.ReadWord(ptb, address, dest)
#define consumeB(address) \
    readB(address);       \
    address++
#define consumeH(address) \
    readH(address);       \
    address += 2
#define consumeW(address) \
    readW(address);       \
    address += 4

#define writeB(address, value) hardware.memory.WriteByte(address, value)
#define writeH(address, value)                      \
    hardware.memory.WriteByte(address, value >> 8); \
    hardware.memory.WriteByte(address + 1, value)
#define writeW(address, value)                             \
    hardware.memory.WriteByte(address + 0, (value >> 24)); \
    hardware.memory.WriteByte(address + 1, (value >> 16)); \
    hardware.memory.WriteByte(address + 2, (value >> 8));  \
    hardware.memory.WriteByte(address + 3, value)

/*
 * this will eventually become the MMU
 * for now, we will just assume everything has access to everything
 * allocate new pages when a new one is needed for read/write
 * never throw any errors or fault conditions
 */

#define PAGE_SIZE 4096
#define PAGES_PER_TABLE (1 << 10)

typedef uint32_t pageAlignedAddress;
#define PAGE_ALIGN(address) static_cast<pageAlignedAddress>(address & 0xfffff000);

class SystemMemory
{
public:
    SystemMemory();
    
    SystemMemory(uint32_t size);

    ~SystemMemory();

    Fault ReadByte(uint32_t ptba, uint32_t address, uint32_t &readTo);

    Fault WriteByte(uint32_t ptba, uint32_t address, uint32_t value);

    Fault ReadHalfWord(uint32_t ptba, uint32_t address, uint32_t &readTo);

    Fault WriteHalfWord(uint32_t ptba, uint32_t address, uint32_t value);

    Fault ReadWord(uint32_t ptba, uint32_t address, uint32_t &readTo);

    Fault WriteWord(uint32_t ptba, uint32_t address, uint32_t value);

    void InitializeFromFile(char *filePath);

private:
    uint8_t *physicalMemory;
    uint32_t physicalMemorySize;

    // arguments: base address of the page table, virtual address, where to write the decoded physical address to
    Fault WalkPageTable(uint32_t ptba, uint32_t va, uint32_t &paTo);
};

#endif
