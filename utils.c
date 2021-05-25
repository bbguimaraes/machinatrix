#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
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

void log_err(const char *fmt, ...) {
    if(PROG_NAME)
        fprintf(LOG_OUT, "%s: ", PROG_NAME);
    if(CMD_NAME)
        fprintf(LOG_OUT, "%s: ", CMD_NAME);
    va_list argp;
    va_start(argp, fmt);
    vfprintf(LOG_OUT, fmt, argp);
    va_end(argp);
}

char *is_prefix(const char *prefix, const char *s) {
    while(*prefix && *s)
        if(*prefix++ != *s++)
            return NULL;
    return *prefix ? NULL : (char *)s;
}

bool copy_arg(const char *name, char *dst, const char *src, size_t max) {
    if(!*src) {
        log_err("empty %s specified\n", name);
        return false;
    }
    for(const char *const end = dst + max;;) {
        if(dst == end) {
            log_err("%s too long (>= %lu)\n", name, max);
            return false;
        }
        if(!(*dst++ = *src++))
            break;
    }
    return true;
}

bool exec(const char *const *argv, int fin, int fout) {
    if(fin) {
        for(;;) {
            if(dup2(fin, STDIN_FILENO) != -1)
                break;
            if(errno == EINTR)
                continue;
            log_err("dup2: %s\n", strerror(errno));
            return false;
        }
        close(fin);
    }
    if(fout) {
        for(;;) {
            if(dup2(fout, STDOUT_FILENO) != -1)
                break;
            if(errno == EINTR)
                continue;
            log_err("dup2: %s\n", strerror(errno));
            return false;
        }
        close(fout);
    }
    if(execvp(argv[0], (char *const *)argv) != -1)
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

void join_lines(unsigned char *b, unsigned char *e) {
    for(; b != e; ++b)
        if(*b == '\n')
            *b = ' ';
}

size_t curl_write_cb(char *in, size_t size, size_t nmemb, TidyBuffer *out) {
    size_t r = size * nmemb;
    tidyBufAppend(out, in, r);
    return r;
}

size_t mtrix_buffer_append(char *p, size_t size, size_t n, mtrix_buffer *b) {
    if(!size || !n)
        return 0;
    size_t r = size * n;
    if(!b->p) {
        b->p = malloc(r + 1);
        memcpy(b->p, p, r);
        b->s = r;
    } else {
        char *n = realloc(b->p, b->s + r + 1);
        if(!n) {
            log_err("realloc\n");
            return 0;
        }
        b->p = n;
        memcpy(b->p + b->s, p, r);
        b->s += r;
    }
    b->p[b->s] = 0;
    return r;
}

bool build_url(char *url, const char *const *v) {
    size_t m = MTRIX_MAX_URL_LEN;
    char *p = url;
    for(; *v; ++v) {
        int l = snprintf(p, m, "%s", *v);
        if(l >= m) {
            log_err("url too long (%zu >= %zu): %s\n", l, m, url);
            return false;
        }
        m -= l;
        p += l;
    }
    return true;
}

static CURL *init_curl(const char *url, mtrix_buffer *b, bool verbose) {
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

bool request(const char *url, mtrix_buffer *b, bool verbose) {
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

bool post(post_request r, bool verbose, mtrix_buffer *b) {
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
