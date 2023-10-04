#include "memmap.hpp"

uint32_t mapAddresses[REGION_COUNT] =
    {
        MEMMAP_IDT,
        MEMMAP_SCREEN,
        MEMMAP_KEYBOARD};