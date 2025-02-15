#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"

#include <tidy.h>
#include <tidybuffio.h>

#include "utils.h"

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

int64_t parse_i64(const char *s) {
    const char *e;
    errno = 0;
    const uint64_t ret = strtoull(s, (char**)&e, 10);
    if(errno) {
        errno = 0;
        fprintf(stderr, "strtoull: %s\n", strerror(errno));
        return -1;
    }
    if(e == s) {
        fprintf(stderr, "failed to parse i64: %s\n", s);
        return -1;
    }
    if(ret > INT64_MAX) {
        fprintf(stderr, "i64 value too large: %" PRIu64 "\n", ret);
        return -1;
    }
    return (int64_t)ret;
}

char *is_prefix(const char *prefix, const char *s) {
    while(*prefix && *s)
        if(*prefix++ != *s++)
            return NULL;
    return *prefix ? NULL : (char *)s;
}

bool copy_arg(const char *name, struct mtrix_buffer dst, const char *src) {
    if(!*src) {
        log_err("empty %s specified\n", name);
        return false;
    }
    for(const char *const end = dst.p + dst.n;;) {
        if(dst.p == end) {
            log_err("%s too long (>= %lu)\n", name, dst.n);
            return false;
        }
        if(!(*dst.p++ = *src++))
            break;
    }
    return true;
}

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

static bool exec_dup(int old, int new) {
    while(dup2(old, new) == -1)
        if(errno != EINTR)
            return log_errno("dup2"), false;
    close(old);
    return true;
}

bool exec(const char *const *argv, int fin, int fout, int ferr) {
    if(fin != -1 && !exec_dup(fin, STDIN_FILENO))
        return false;
    if(fout != -1 && !exec_dup(fout, STDOUT_FILENO))
        return false;
    if(ferr != -1 && !exec_dup(ferr, STDERR_FILENO))
        return false;
    if(execvp(argv[0], (char *const *)argv) != -1)
        return true;
    log_errno("execvp");
    return false;
}

bool wait_n(size_t n, const pid_t *p) {
    bool ret = true;
    for(size_t i = 0; i < n; ++i) {
        int status;
        if(waitpid(p[i], &status, 0) == -1) {
            log_errno("wait");
            ret = false;
        } else if(WIFSIGNALED(status)) {
            --n;
            log_err("child killed by signal: %d\n", WTERMSIG(status));
            ret = false;
        } else if(WIFEXITED(status)) {
            --n;
            if((status = WEXITSTATUS(status))) {
                log_err("child exited: %d\n", status);
                ret = false;
            }
        }
    }
    return ret;
}

void join_lines(unsigned char *b, unsigned char *e) {
    for(; b != e; ++b)
        if(*b == '\n')
            *b = ' ';
}

size_t curl_write_cb(char *in, size_t size, size_t nmemb, TidyBuffer *out) {
    size_t r = size * nmemb;
    assert(r <= UINT_MAX);
    tidyBufAppend(out, in, (unsigned)r);
    return r;
}

size_t mtrix_buffer_append(
    const char *p, size_t size, size_t n, struct mtrix_buffer *b
) {
    if(!size || !n)
        return 0;
    size_t r = size * n;
    if(!b->p) {
        b->p = malloc(r + 1);
        memcpy(b->p, p, r);
        b->n = r;
    } else {
        char *n = realloc(b->p, b->n + r + 1);
        if(!n) {
            log_err("realloc\n");
            return 0;
        }
        b->p = n;
        memcpy(b->p + b->n, p, r);
        b->n += r;
    }
    b->p[b->n] = 0;
    return r;
}

bool build_url(char *url, const char *const *v) {
    size_t m = MTRIX_MAX_URL_LEN;
    char *p = url;
    for(; *v; ++v) {
        const int l = snprintf(p, m, "%s", *v);
        if(l < 0) {
            log_err("build_url: snprintf");
            return false;
        }
        const size_t ul = (size_t)l;
        if(ul >= m) {
            log_err("url too long (%zu >= %zu): %s\n", l, m, url);
            return false;
        }
        m -= ul;
        p += l;
    }
    return true;
}

static CURL *init_curl(const char *url, struct mtrix_buffer *b, bool verbose) {
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "machinatrix");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mtrix_buffer_append);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, b);
    if(verbose)
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    return curl;
}

bool request(const char *url, struct mtrix_buffer *b, bool verbose) {
    CURL *curl = init_curl(url, b, verbose);
    char err[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err);
    if(verbose)
        printf("Request: GET %s\n", url);
    CURLcode ret = curl_easy_perform(curl);
    if(ret != CURLE_OK)
        log_err("%d: %s: %s\n", ret, err, *err ? err : curl_easy_strerror(ret));
    else if(verbose)
        printf("Response:\n%s\n", b->p);
    curl_easy_cleanup(curl);
    return ret == CURLE_OK;
}

bool post(post_request r, bool verbose, struct mtrix_buffer *b) {
    CURL *curl = init_curl(r.url, b, verbose);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, r.data_len);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, r.data);
    if(verbose)
        printf("Request: POST %s\n", r.url);
    bool ret = !curl_easy_perform(curl);
    if(verbose && ret)
        printf("Response:\n%s\n", b->p);
    curl_easy_cleanup(curl);
    return ret;
}
