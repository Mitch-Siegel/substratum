#include <iostream>
#include <chrono>
#include <fstream>
#include <thread>

#include "names.hpp"
#include "memory.hpp"
#include "hardware.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ncurses.h>
#include <chrono>

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

void cleanupncurses()
{
    if (OK != endwin())
    {
        printf("Error calling endwin()\n");
    }
}

int tickRate = 0;
WINDOW *infoWin = nullptr;
WINDOW *consoleWin = nullptr;

void *HardwareThread(void *params)
{
    std::chrono::steady_clock::time_point intervalStart = std::chrono::steady_clock::now();
    int64_t leftoverMicros = 0;
    while (hardware.Running())
    {
        if (tickRate)
        {
            hardware.Tick();
            mvwprintw(infoWin, 0, 0, "IP: %08x (%d instructions/sec) %ld", hardware.GetCore(0).ConfigRegisters()[ip], tickRate, leftoverMicros);
            std::chrono::steady_clock::time_point intervalEnd = std::chrono::steady_clock::now();
            int64_t intervalDuration = std::chrono::duration_cast<std::chrono::microseconds>(intervalEnd - intervalStart).count();
            leftoverMicros += ((1000000.0f / tickRate) - intervalDuration);
            if (leftoverMicros > 100)
            {
                usleep(leftoverMicros);
                leftoverMicros = 0;
            }
            intervalStart = std::chrono::steady_clock::now();
        }
        else
        {
            mvwprintw(infoWin, 0, 0, "IP: %08x (%d instructions/sec) %ld", hardware.GetCore(0).ConfigRegisters()[ip], tickRate, leftoverMicros);
            usleep(100000);
        }
    }

    return nullptr;
}

int main(int argc, char *argv[])
{
    initscr();
    nodelay(stdscr, true);  // give us keypresses as soon as possible
    // keypad(stdscr, TRUE);   // interpret special keys (arrow keys and such)
    noecho();               // don't automatically print back what we type
    atexit(cleanupncurses);

    consoleWin = newwin(LINES - 1, COLS, 0, 0);
    keypad(consoleWin, TRUE);   // interpret special keys (arrow keys and such)
    scrollok(consoleWin, TRUE); // we want to scroll the console
    infoWin = newwin(1, COLS, LINES - 1, 0);

    wprintw(infoWin, "INFORMATION WINDOW :)\n");
    box(infoWin, '*', '*');
    wrefresh(infoWin);
    wrefresh(stdscr);

    if (argc < 1)
    {
        std::cout << "Please provide bin file to read asm from!" << std::endl;
        exit(1);
    }
    hardware.memory.InitializeFromFile(argv[1]);

    hardware.Start();
    pthread_t hwThread;
    pthread_create(&hwThread, nullptr, HardwareThread, nullptr);

    int ch = 0;

    uint32_t instructionCount = 0;
    while (hardware.Running())
    {
        if (-1 != (ch = wgetch(consoleWin)))
        {
            switch (ch)
            {
            case KEY_UP:
                if (!tickRate)
                {
                    tickRate = 1;
                }
                else
                {
                    tickRate *= 2;
                }
                break;

            case KEY_DOWN:
                if (tickRate)
                {
                    if (tickRate == 1)
                    {
                        tickRate = 0;
                    }
                    else
                    {
                        tickRate /= 2;
                    }
                }
                break;

            default:
                // mvwprintw(infoWin, 0, 25, "%c:%d", ch, ch);
                break;
            }
        }
        else
        {
            usleep((1000000.0 / 60.0));
        }
        wrefresh(stdscr);
        wrefresh(infoWin);
    }

    pthread_join(hwThread, nullptr);

    printw("Press any key to exit...\n");
    nodelay(stdscr, false); // block waiting to exit
    getch();

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