#include <ncurses.h>
#include <pthread.h>
#include <set>

#ifndef _UI_HPP_
#define _UI_HPP_

extern WINDOW *infoWin;
extern WINDOW *consoleWin;
extern WINDOW *insViewWin;
extern WINDOW *coreStateWin;

class UI
{
public:
    UI();

    ~UI();

    void Refresh();

    int mvwprintw_threadsafe(WINDOW *w, int y, int x, const char *fmt, ...);

    int wprintw_threadsafe(WINDOW *w, const char *fmt, ...);

private:
    std::set<WINDOW *> toRefresh;
    
    void Lock();

    void Unlock();

    pthread_mutex_t lock;
};

extern UI ui;

#endif