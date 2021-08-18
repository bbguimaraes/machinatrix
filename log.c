#include "log.h"

#include <errno.h>
#include <stdarg.h>
#include <string.h>

static FILE *LOG_OUT = NULL;

FILE *log_set(FILE *f) {
    FILE *ret = LOG_OUT;
    LOG_OUT = f;
    return ret;
}

void log_errv(const char *fmt, va_list argp) {
    if(PROG_NAME)
        fprintf(LOG_OUT, "%s: ", PROG_NAME);
    if(CMD_NAME)
        fprintf(LOG_OUT, "%s: ", CMD_NAME);
    vfprintf(LOG_OUT, fmt, argp);
}

void log_err(const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    log_errv(fmt, argp);
    va_end(argp);
}

void log_errno(const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    log_errv(fmt, argp);
    va_end(argp);
    fprintf(LOG_OUT, ": %s\n", strerror(errno));
    errno = 0;
}
