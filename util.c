#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
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

bool exec(const char *const *argv) {
    if(execvp(argv[0], (char *const*)argv) != -1)
        return true;
    log_err("execvp: %s\n", strerror(errno));
    return false;
}

bool wait_n(size_t n) {
    bool ret = true;
    while(n) {
        int status;
        pid_t w = wait(&status);
        if(w == -1) {
            log_err("wait: %s\n", strerror(errno));
            ret = false;
        } else if(WIFSIGNALED(status)) {
            --n;
            log_err("child killed by signal: %d\n", WTERMSIG(status));
            ret = false;
        } else if(WIFEXITED(status)) {
            --n;
            if((status = WEXITSTATUS(status))) {
                log_err("child exited: %d\n", WEXITSTATUS(status));
                ret = false;
            }
        }
    }
    return ret;
}
