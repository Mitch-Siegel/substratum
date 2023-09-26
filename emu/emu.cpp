#include <iostream>
#include <chrono>
#include <fstream>
#include <thread>

#include "names.hpp"
#include "memory.hpp"
#include "hardware.hpp"
// #define PRINTEXECUTION

// uint8_t memory[0x10000] = {0};

// void printState()
// {
//     for (int row = 0; row < 2; row++)
//     {
//         for (int i = 0; i < (9 - row); i++)
//         {
//             printf("%8s|", Core::registerNames[(9 * row) + i].c_str());
//         }
//         std::cout << std::endl;
//         for (int i = 0; i < (9 - row); i++)
//         {
//             printf("%8x|", Core::Registers[(9 * row) + i]);
//         }
//         std::cout << std::endl;
//     }
//     printf("\nNF: %d ZF: %d CF: %d VF: %d\n", Flags[NF], Flags[ZF], Flags[CF], Flags[VF]);

//     /*uint32_t stackScan = 0x10000;
//     while (stackScan > (uint32_t)Registers[sp])
//     {
//         uint16_t val = readWord(stackScan - 1);
//         printf("%04x: %05d // %04x\n", stackScan - 1, val, val);
//         stackScan -= 2;
//     }
//     std::cout << std::endl;
//     */
// }

int main(int argc, char *argv[])
{
    if (argc < 1)
    {
        std::cout << "Please provide bin file to read asm from!" << std::endl;
        exit(1);
    }
    hardware.memory.InitializeFromFile(argv[1]);

    hardware.Start();

    uint32_t instructionCount = 0;
    while (hardware.Running())
    {
        hardware.Tick();
    }

    // printState();
    std::cout << "Execution halted after " << instructionCount << " instructions" << std::endl;
    std::cout << "opening dump file" << std::endl;
    std::ofstream dumpFile;
    dumpFile.open("memdump.bin", std::ofstream::out);
    std::cout << "dump file opened" << std::endl;
    for (uint32_t pageIndex : hardware.memory.ActivePages())
    {
        char pageHeader[18];
        snprintf(pageHeader, 17, "Page add%08x", pageIndex << PAGE_BIT_WIDTH);
        dumpFile << pageHeader;
        for (int i = 0; i < PAGE_SIZE; i++)
        {
            dumpFile.put(hardware.memory.ReadByte((pageIndex << PAGE_BIT_WIDTH) + i));
        }
    }
    dumpFile.close();
}