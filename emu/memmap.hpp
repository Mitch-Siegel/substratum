#include <cstdint>

#ifndef _MEMMAP_HPP_
#define _MEMMAP_HPP_

struct IDT
{
    uint32_t vectors[256];
};

struct ScreenMem
{
    uint8_t rows[24][80];
};

struct KeyboardIn
{
    uint8_t keyPressed;
};

struct MEMMAP_RESERVE
{
    struct IDT idt;
    struct ScreenMem screen;
    struct KeyboardIn keyboard;
};

enum class MappedRegions
{
    idt,
    screen,
    keyboard,
    REGION_COUNT = keyboard + 1,
};

#define MEMMAP_BASE (0x00000100)
#define MEMMAP_IDT (MEMMAP_BASE) // 0x0 offset
#define MEMMAP_SCREEN (MEMMAP_IDT + sizeof(struct IDT)) // 0x0 + 0x400 = 0x400 offset
#define MEMMAP_KEYBOARD (MEMMAP_SCREEN + sizeof(struct ScreenMem)) // 0x400 + 0x780 = 0B80 offset
#define REGION_COUNT 3
#define MEMMAP_RESERVED_SIZE (sizeof(struct IDT) + sizeof(struct ScreenMem) + sizeof(struct KeyboardIn));

extern uint32_t mapAddresses[REGION_COUNT];

#endif
