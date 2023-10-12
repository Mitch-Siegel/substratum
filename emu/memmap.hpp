#include <cstdint>

#ifndef _MEMMAP_HPP_
#define _MEMMAP_HPP_

struct IDT
{
    uint32_t vectors[256];
};

struct UartMem
{
    uint8_t xmit; // outbound byte (from vm's perspective)
    uint8_t rcv;  // inbound byte  (from vm's perspective)
};

struct MEMMAP_RESERVE
{
    struct IDT idt;
    struct UartMem screen;
};

enum class MappedRegions
{
    idt,
    uart,
    REGION_COUNT = uart + 1,
};

#define MEMMAP_BASE (0x00000100)
#define MEMMAP_IDT (MEMMAP_BASE)                      // 0x0 offset
#define MEMMAP_UART (MEMMAP_IDT + sizeof(struct IDT)) // 0x0 + 0x400 = 0x400 offset
#define REGION_COUNT 2
#define MEMMAP_RESERVED_SIZE (sizeof(struct IDT) + sizeof(struct UartMem));

extern uint32_t mapAddresses[REGION_COUNT];

#endif
