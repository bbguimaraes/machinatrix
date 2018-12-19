#include <stdbool.h>

void log_err(const char *fmt, ...);
bool copy_arg(const char *name, char *dst, const char *src, size_t max);
bool exec(const char *const *argv);
bool wait_n(size_t n);
