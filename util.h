#include <stdbool.h>

void log_err(const char *fmt, ...);
bool copy_arg(const char *name, char *dst, const char *src, size_t max);
