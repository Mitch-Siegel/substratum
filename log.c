#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include "ast.h"

enum LOG_LEVEL logLevel = LOG_WARNING;

char *logLevelNames[LOG_FATAL + 1] =
    {
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR",
        "FATAL"};

void set_log_level(enum LOG_LEVEL newLevel)
{
    logLevel = newLevel;
    log(LOG_INFO, "Set log level to %s", logLevelNames[newLevel]);
}

void print_log_level(enum LOG_LEVEL level, const char *file, size_t line)
{
    switch (level)
    {
    case LOG_DEBUG:
        printf("[  DEBUG] ");
        break;
    case LOG_INFO:
        printf("[   INFO] ");
        break;
    case LOG_WARNING:
        printf("[WARNING] ");
        break;
    case LOG_ERROR:
        printf("[  ERROR] ");
        break;
    case LOG_FATAL:
        // TODO: only log as [ FATAL ] if configured to do so
        // printf("[ FATAL ]");
        break;
    }
}

void log_function(const char *file, size_t line, enum LOG_LEVEL level, const char *format, ...)
{
    if (level < logLevel)
    {
        return;
    }

    va_list args;
    va_start(args, format);
    print_log_level(level, file, line);
    vprintf(format, args);
    putc('\n', stdout);
    va_end(args);

    if (level == LOG_FATAL)
    {
        exit(1);
    }
}

void log_tree_function(const char *file, size_t line, enum LOG_LEVEL level, struct Ast *tree, const char *format, ...)
{
    if (level < logLevel)
    {
        return;
    }

    va_list args;
    va_start(args, format);
    print_log_level(level, file, line);
    // TODO: option to print tree even on fatal
    printf("%s:%d:%d: ", tree->sourceFile, tree->sourceLine, tree->sourceCol);
    vprintf(format, args);
    putc('\n', stdout);
    va_end(args);

    if (level == LOG_FATAL)
    {
        exit(1);
    }
}
