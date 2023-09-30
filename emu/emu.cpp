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
WINDOW *infoWin = nullptr;
WINDOW *consoleWin = nullptr;
WINDOW *insViewWin = nullptr;
WINDOW *coreStateWin = nullptr;

void printStatus()
{
    ui.mvwprintw_threadsafe(infoWin, 0, 0, "[%3s] IP: %08x",
                            (keymod ? "MOD" : "   "),
                            hardware.GetCore(0).ConfigRegisters()[static_cast<std::underlying_type_t<enum ConfigRegisters>>(ConfigRegisters::ip)]);

    ui.wprintw_threadsafe(infoWin, "tickrate: ");
    if (flatout)
    {
        ui.wprintw_threadsafe(infoWin, "FLATOUT");
    }
    else
    {
        ui.wprintw_threadsafe(infoWin, "%-7d", tickRate);
    }
}

void *HardwareThread(void *params)
{
    int64_t leftoverMicros = 0;
    std::chrono::steady_clock::time_point intervalStart = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastStatusPrint = intervalStart;
    while (hardware.Running())
    {
        if (tickRate || flatout)
        {
            hardware.Tick();
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
    nodelay(consoleWin, true);  // give us keypresses as soon as possible
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
    int64_t leftoverMicros = 0;
    std::chrono::steady_clock::time_point intervalStart = std::chrono::steady_clock::now();

    wclear(stdscr);
    wrefresh(consoleWin);
    wrefresh(infoWin);
    wrefresh(coreStateWin);
    wrefresh(insViewWin);
    refresh();

    while (hardware.Running())
    {
        ch = wgetch(consoleWin);

        if (0 < ch)
        {
            ui.mvwprintw_threadsafe(infoWin, 0, 45, "%4d:%3c", ch, ch);
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

                // case 27:
                // hardware.Stop();
                // break;

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
                hardware.memory->MappedKeyboard()->keyPressed = ch & 0xff;
                break;
            }
        }
        else
        {
            hardware.memory->MappedKeyboard()->keyPressed = 0;
        }

        char scrBuf[(81 * 24) + 1];
        uint16_t destA = 0;
        struct ScreenMem *s = hardware.memory->MappedScreen();
        for(uint8_t srcRow = 0; srcRow < 24; srcRow++)
        {
            memcpy(scrBuf + destA, s->rows[srcRow], 80);
            destA += 80;
            scrBuf[destA++] = '\n';
        }
        scrBuf[destA] = '\0';
        ui.mvwprintw_threadsafe(consoleWin, 0, 0, "%s", scrBuf);

        ui.Refresh();

        std::chrono::steady_clock::time_point intervalEnd = std::chrono::steady_clock::now();
        int64_t intervalDuration = std::chrono::duration_cast<std::chrono::microseconds>(intervalEnd - intervalStart).count();
        double tickDuration = (1000000.0f / 600.0f);
        leftoverMicros += (tickDuration - intervalDuration);
        if (leftoverMicros > 100)
        {
            usleep(leftoverMicros);
            leftoverMicros = 0;
        }
        // else if (leftoverMicros < (1000000.0f / 60.0f))
        // {
        // leftoverMicros = 0;
        // }
        intervalStart = std::chrono::steady_clock::now();
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