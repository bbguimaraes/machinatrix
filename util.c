#include <stdarg.h>
#include <stdio.h>
#include "util.h"

extern const char *PROG_NAME, *CMD_NAME;

void log_err(const char *fmt, ...) {
    if(PROG_NAME)
        fprintf(stderr, "%s: ", PROG_NAME);
    if(CMD_NAME)
        fprintf(stderr, "%s: ", CMD_NAME);
    va_list argp;
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
}

bool copy_arg(const char *name, char *dst, const char *src, size_t max) {
    if(!*src) {
        log_err("empty %s specified\n", name);
        return false;
    }
    size_t i = 0;
    while(i < max && *src)
        dst[i++] = *src++;
    if(i >= max) {
        log_err("%s too long (>= %lu)\n", name, max);
        return false;
    }
    dst[i] = '\0';
    return true;
}
