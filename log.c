#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include "ast.h"

void printLogLevel(enum LogLevel level, const char *file, size_t line)
{
    switch (level)
    {
    case LOG_DEBUG:
        printf("[DEBUG@%s:%zu] ", file, line);
        break;
    case LOG_INFO:
        printf("[INFO@%s:%zu] ", file, line);
        break;
    case LOG_WARNING:
        printf("[WARNING@%s:%zu] ", file, line);
        break;
    case LOG_ERROR:
        printf("[ERROR@%s:%zu] ", file, line);
        break;
    case LOG_FATAL:
        printf("[FATAL@%s:%zu] ", file, line);
        break;
    }
}

void LogFunction(const char *file, size_t line, enum LogLevel level, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printLogLevel(level, file, line);
    vprintf(format, args);
    putc('\n', stdout);
    va_end(args);

    if(level == LOG_FATAL)
    {
        exit(1);
    }
}

void LogTreeFunction(const char *file, size_t line, enum LogLevel level, struct AST *tree, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printLogLevel(level, file, line);
    printf("%s:%d:%d: ", tree->sourceFile, tree->sourceLine, tree->sourceCol);
    vprintf(format, args);
    putc('\n', stdout);
    va_end(args);

    if(level == LOG_FATAL)
    {
        exit(1);
    }
}
