#include "os.h"

#include <unistd.h>

#include "log.h"

char *join_path(char v[static MTRIX_MAX_PATH], int n, ...) {
    va_list args;
    va_start(args, n);
    char *p = v, *const e = v + MTRIX_MAX_PATH;
    for(int i = 0; i != n; ++i) {
        size_t ni = strlcpy(p, va_arg(args, const char*), (size_t)(e - p));
        if(e[-1])
            goto err;
        p += ni - 1;
    }
    assert(p <= e);
    assert(!e[-1]);
    va_end(args);
    return v;
err:
    va_end(args);
    log_err("%s: path too long: %s\n", __func__, v);
    return NULL;
}

FILE *open_or_create(const char *path, const char *flags) {
    FILE *ret;
    if(!(ret = fopen(path, "a")))
        return log_errno("failed to open file %s", path), NULL;
    if(fclose(ret) == -1)
        return log_errno("failed to close file %s", path), NULL;
    if(!(ret = fopen(path, flags)))
        return log_errno("failed to open file %s", path), NULL;
    return ret;
}

bool read_all(int fd, void *p, size_t n) {
    while(n) {
        const ssize_t nr = read(fd, p, n);
        switch(nr) {
        case -1: return log_errno("read"), false;
        case 0: return log_err("short read\n"), false;
        default: p = (char*)p + nr, n -= (size_t)nr;
        }
    }
    return true;
}

bool write_all(int fd, const void *p, size_t n) {
    while(n) {
        const ssize_t nw = write(fd, p, n);
        switch(nw) {
        case -1: return log_errno("write"), false;
        default: p = (char*)p + nw, n -= (size_t)nw;
        }
    }
    return true;
}

bool fread_all(FILE *f, void *p, size_t n) {
    int fd = fileno(f);
    if(fd == -1) {
        log_errno("fileno");
        return false;
    }
    return read_all(fd, p, n);
}

bool fwrite_all(FILE *f, const void *p, size_t n) {
    int fd = fileno(f);
    if(fd == -1) {
        log_errno("fileno");
        return false;
    }
    return write_all(fd, p, n);
}
