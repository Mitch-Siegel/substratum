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

void cleanupncurses()
{
    if (OK != endwin())
    {
        printf("Error calling endwin()\n");
    }
}

uint32_t tickRate = 0; // how fast we tick the system
bool keymod;           // modifier key (sticky) so that we can use normal letters/numbers to control things
bool flatout;          // run the emulator flat-out, tick as fast as possible
// when we tick our time will be slightly off, track this and make up for it
int64_t leftoverMicros = 0;
WINDOW *infoWin = nullptr;
WINDOW *consoleWin = nullptr;
WINDOW *insViewWin = nullptr;
WINDOW *coreStateWin = nullptr;

void printStatus()
{
    mvwprintw(infoWin, 0, 0, "[%3s] IP: %08x",
              (keymod ? "MOD" : "   "),
              hardware.GetCore(0).ConfigRegisters()[static_cast<std::underlying_type_t<enum ConfigRegisters>>(ConfigRegisters::ip)]);

    wprintw(infoWin, "tickrate: ");
    if (flatout)
    {
        wprintw(infoWin, "FLATOUT");
    }
    else
    {
        wprintw(infoWin, "%-7d", tickRate);
    }

    wrefresh(infoWin);
}

void *HardwareThread(void *params)
{
    std::chrono::steady_clock::time_point intervalStart = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastStatusPrint = intervalStart;
    while (hardware.Running())
    {
        if (tickRate || flatout)
        {
            hardware.Tick();
            wrefresh(insViewWin);
            if (!flatout)
            {
                std::chrono::steady_clock::time_point intervalEnd = std::chrono::steady_clock::now();
                int64_t intervalDuration = std::chrono::duration_cast<std::chrono::microseconds>(intervalEnd - intervalStart).count();
                double tickDuration = (1000000.0f / tickRate);
                leftoverMicros += (tickDuration - intervalDuration);
                if (leftoverMicros > 100)
                {
                    usleep(leftoverMicros);
                    leftoverMicros = 0;
                }
                else if (leftoverMicros < (-1000 * tickDuration))
                {
                    leftoverMicros = 0;
                    tickRate /= 2;
                }

                if (1000000 < (intervalEnd - lastStatusPrint).count())
                {
                    lastStatusPrint = intervalEnd;
                    printStatus();
                }
                intervalStart = std::chrono::steady_clock::now();
            }
        }
        else
        {
            printStatus();
            usleep(100000);
        }
    }

    return nullptr;
}

int main(int argc, char *argv[])
{

    // wprintw(infoWin, "INFORMATION WINDOW :)\n");
    // box(infoWin, '*', '*');
    // wrefresh(infoWin);
    // wrefresh(stdscr);

    if (argc < 1)
    {
        std::cout << "Please provide bin file to read asm from!" << std::endl;
        exit(1);
    }
    hardware.memory->InitializeFromFile(argv[1]);

    initscr();
    nodelay(stdscr, true); // give us keypresses as soon as possible
    // keypad(stdscr, TRUE);   // interpret special keys (arrow keys and such)
    noecho(); // don't automatically print back what we type
    atexit(cleanupncurses);

    consoleWin = newwin(LINES - 1, COLS - 40, 0, 0);
    keypad(consoleWin, TRUE);   // interpret special keys (arrow keys and such)
    scrollok(consoleWin, TRUE); // we want to scroll the console
    infoWin = newwin(1, COLS, LINES - 1, 0);

    insViewWin = newwin((LINES - 1) - 10, 45,
                        0, COLS - 45);
    coreStateWin = newwin(10, 45,
                          (LINES - 1) - 10, COLS - 45);
    scrollok(insViewWin, TRUE); // we want to scroll the instruction view

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
                if (flatout)
                {
                    flatout = false;
                    printStatus();
                    break;
                }

                if (keymod)
                {
                    flatout = true;
                    keymod = false;
                    printStatus();
                    break;
                }

                if (!tickRate)
                {
                    tickRate = 1;
                }
                else
                {
                    tickRate *= 2;
                }
                printStatus();
                break;

            case KEY_DOWN:
                if (flatout)
                {
                    flatout = false;
                    printStatus();
                    break;
                }

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
                printStatus();
                break;

            case KEY_RIGHT:
            case KEY_LEFT:
                keymod = !keymod;
                printStatus();
                break;

            case 27:
                hardware.Stop();
                break;

            case '0':
                tickRate = 0;
                keymod = false;
                printStatus();
                break;

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (keymod)
                {
                    tickRate = 1;
                    uint8_t intermediate = ch - '0';
                    while (intermediate--)
                    {
                        tickRate *= 3;
                    }
                    keymod = false;
                    printStatus();
                    break;
                }

            default:
                hardware.memory->MappedKeyboard()->keyPressed = 'A';
                mvwprintw(infoWin, 0, 45, "%c:%d", ch, ch);
                break;
            }
        }
        else
        {
            wrefresh(infoWin);
            wrefresh(consoleWin);
            usleep((1000000.0 / 60.0));
        }
    }

    pthread_join(hwThread, nullptr);

    printw("Press any key to exit...\n");
    nodelay(stdscr, false); // block waiting to exit
    getch();

    // printState();
    std::cout << "Execution halted after " << instructionCount << " instructions" << std::endl;
    std::cout << "opening dump file" << std::endl;
    std::ofstream dumpFile;

    /*dumpFile.open("memdump.bin", std::ofstream::out);
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
    dumpFile.close();*/
}