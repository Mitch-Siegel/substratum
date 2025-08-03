#ifndef LOG_H
#define LOG_H

#include "substratum_defs.h"

struct Ast;

enum LOG_LEVEL
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL
};

void set_log_level(enum LOG_LEVEL newLevel);

void log_function(const char *file, size_t line, enum LOG_LEVEL level, const char *format, ...);

void log_tree_function(const char *file, size_t line, enum LOG_LEVEL level, struct Ast *tree, const char *format, ...);

#define log(level, format, ...) log_function(__FILE__, __LINE__, level, format, ##__VA_ARGS__)
#define log_tree(level, tree, format, ...) log_tree_function(__FILE__, __LINE__, level, tree, format, ##__VA_ARGS__)
#define InternalError(format, ...)                                      \
    log_function(__FILE__, __LINE__, LOG_FATAL, format, ##__VA_ARGS__); \
    exit(1)

#endif
