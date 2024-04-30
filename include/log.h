#ifndef LOG_H
#define LOG_H

#include "substratum_defs.h"

struct AST;

enum LogLevel
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL
};

void setLogLevel(enum LogLevel newLevel);

void LogFunction(const char *file, size_t line, enum LogLevel level, const char *format, ...);

void LogTreeFunction(const char *file, size_t line, enum LogLevel level, struct AST *tree, const char *format, ...);

void InternalErrorFunction(const char *file, size_t line, const char *format, ...);

#define Log(level, format, ...) LogFunction(__FILE__, __LINE__, level, format, ##__VA_ARGS__)
#define LogTree(level, tree, format, ...) LogTreeFunction(__FILE__, __LINE__, level, tree, format, ##__VA_ARGS__)
#define InternalError(format, ...)                                     \
    LogFunction(__FILE__, __LINE__, LOG_FATAL, format, ##__VA_ARGS__); \
    exit(1)

#endif
