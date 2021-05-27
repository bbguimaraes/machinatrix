#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

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
#define CHECK_LOG(x) check_log(LOC(), (x))

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

static inline bool check_log(struct loc loc, const char *s) {
    FILE *f = log_set(NULL);
    log_set(f);
    assert(fflush(f) != EOF);
    long n = ftell(f);
    assert(n > 0);
    assert(fseek(f, 0, SEEK_SET) >= 0);
    char *text = malloc((size_t)n);
    fread(text, 1, (size_t)n, f);
    assert(!ferror(f));
    const bool ret = strncmp(text, s, strlen(s) - 1) == 0;
    if(!ret) {
        print_loc(loc);
        fprintf(stderr, "unexpected log output: %.*s", (int)n, text);
    }
    free(text);
    return ret;
}
