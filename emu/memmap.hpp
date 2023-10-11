#include <cstdint>

#ifndef _MEMMAP_HPP_
#define _MEMMAP_HPP_

struct IDT
{
    uint32_t vectors[256];
};

// uart statuses (from perspective of the vm)
#define UART_READY 0  // uart is idle
#define UART_INPUT 1  // char sending to uart
#define UART_OUTPUT 2 // char waiting from uart
struct UartMem
{
    uint8_t xr; // transmitted/received byte
    uint8_t status;
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
