#include "ui.hpp"

#include <stdlib.h>

UI ui;

UI::UI()
{
    pthread_mutex_init(&this->lock, nullptr);
}

UI::~UI()
{
    pthread_mutex_destroy(&this->lock);
}

void UI::Refresh()
{
    this->Lock();
    for (auto w : this->toRefresh)
    {
        wrefresh(w);
    }
    refresh();
    this->toRefresh.clear();
    this->Unlock();
}

int UI::mvwprintw_threadsafe(WINDOW *w, int y, int x, const char *fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    this->Lock();
    this->toRefresh.insert(w);
    int retv = wmove(w, y, x);
    if (0 > retv)
    {
        return retv;
    }
    retv = vwprintw(w, fmt, v);
    this->Unlock();
    va_end(v);
    return retv;
}

int UI::wprintw_threadsafe(WINDOW *w, const char *fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    this->Lock();
    this->toRefresh.insert(w);
    int retv = vwprintw(w, fmt, v);
    this->Unlock();
    va_end(v);
    return retv;
}

void UI::Lock()
{
    if (0 > pthread_mutex_lock(&this->lock))
    {
        printf("Mutex error in UI::Lock\n");
        exit(-1);
    }
}

void UI::Unlock()
{
    if (0 > pthread_mutex_unlock(&this->lock))
    {
        printf("Mutex error in UI::Lock\n");
        exit(-1);
    }
}
