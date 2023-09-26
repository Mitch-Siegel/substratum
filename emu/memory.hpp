
#include <cstdint>
#include <map>
#include <set>

#ifndef _MEMORY_HPP_
#define _MEMORY_HPP_

#define readB(address) hardware.memory.ReadByte(address);
#define readH(address) ((hardware.memory.ReadByte(address) << 8) + hardware.memory.ReadByte(address + 1))
#define readW(address) ((hardware.memory.ReadByte(address) << 24) + (hardware.memory.ReadByte(address + 1) << 16) + (hardware.memory.ReadByte(address + 2) << 8) + hardware.memory.ReadByte(address + 3))
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
#define writeH(address, value)             \
    hardware.memory.WriteByte(address, value >> 8); \
    hardware.memory.WriteByte(address + 1, value)
#define writeW(address, value)                    \
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
#define PAGE_BIT_WIDTH 12

typedef uint32_t pageAlignedAddress;
#define PAGE_ALIGN(address) static_cast<pageAlignedAddress>(address & 0xfffff000);

struct Page
{
    uint8_t data[PAGE_SIZE];
};

class SystemMemory
{
private:
    std::map<pageAlignedAddress, Page *> pages;
    std::set<uint32_t> activePages;

public:
    uint8_t ReadByte(uint32_t address);

    void WriteByte(uint32_t address, uint8_t value);

    const std::set<uint32_t> &ActivePages();

    void InitializeFromFile(char *filePath);
};

#endif
