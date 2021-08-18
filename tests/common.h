#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#define RUN(x) run(#x, (x))
#define LOC() \
    (struct loc) { __FILE__, __func__, __LINE__ }
#define FAIL(n, s0, s1) (print_loc(LOC()), fail(n, s0, s1))
#define ASSERT(x) ((x) || FAIL(-1, #x, NULL))
#define ASSERT_EQ(x, y) ((x) == (y) || FAIL(-1, #x " == " #y, NULL))
#define ASSERT_NE(x, y) ((x) != (y) || FAIL(-1, #x " != " #y, NULL))
#define ASSERT_STR_EQ(x, y) ((strcmp((x), (y)) == 0) || FAIL(-1, (x), (y)))
#define ASSERT_STR_EQ_N(x, y, n) \
    ((strncmp((x), (y), (n)) == 0) || FAIL((int)(n), (x), (y)))
#define CHECK_LOG(x) check_log(LOC(), (x), 0)
#define CHECK_LOG_N(x, n) check_log(LOC(), (x), (n))

struct loc {
    const char *file, *func;
    int line;
};

static inline bool run(const char *name, bool (*f)()) {
    assert(printf("%s: ", name) > 0);
    assert(fflush(stdout) != EOF);
    const bool ret = f();
    assert(printf(ret ? "ok\n" : "fail\n") > 0);
    return ret;
}

static inline void print_loc(struct loc loc) {
    fprintf(stderr, "%s:%d: %s: ", loc.file, loc.line, loc.func);
}

static inline bool fail(int n, const char *s0, const char *s1) {
    if(!s1) {
        if(n == -1)
            fprintf(stderr, "assertion failed: %s\n", s0);
        else
            fprintf(stderr, "assertion failed: %.*s\n", n, s0);
    } else {
        if(n == -1)
            fprintf(stderr, "assertion failed: %s == %s\n", s0, s1);
        else
            fprintf(stderr, "assertion failed: %.*s == %.*s\n", n, s0, n, s1);
    }
    return false;
}

static inline bool check_log(struct loc loc, const char *s, size_t len) {
    FILE *f = log_set(NULL);
    log_set(f);
    assert(fflush(f) != EOF);
    long log_len = ftell(f);
    assert(log_len > 0);
    assert(fseek(f, 0, SEEK_SET) >= 0);
    char *text = malloc((size_t)log_len);
    fread(text, 1, (size_t)log_len, f);
    assert(!ferror(f));
    size_t check_len = len ? len : strlen(s);
    size_t min_len = check_len <= (size_t)log_len ? check_len : (size_t)log_len;
    const bool ret =
        strncmp(text, s, min_len) == 0
        && (len || (size_t)log_len == check_len);
    if(!ret) {
        print_loc(loc);
        fprintf(stderr, "unexpected log output: %.*s", (int)log_len, text);
    }
    free(text);
    return ret;
}
