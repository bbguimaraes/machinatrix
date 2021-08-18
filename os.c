#include "os.h"

#include <unistd.h>

#include "log.h"

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
