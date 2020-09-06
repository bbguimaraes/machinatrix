#include "utils.h"

#define RUN(x) run(#x, (x))
#define ASSERT(x) assert_fail((x), __FILE__, __LINE__, #x)
#define ASSERT_EQ(x, y) \
    assert_eq_fail((x) == (y), __FILE__, __LINE__, #x, #y)
#define ASSERT_NE(x, y) \
    assert_eq_fail((x) != (y), __FILE__, __LINE__, #x, #y)
#define ASSERT_STR_EQ(x, y) \
    assert_eq_fail(strcmp(x, y) == 0, __FILE__, __LINE__, x, y)
#define CHECK_LOG(x) check_log(__FILE__, __LINE__, (x))

bool run(const char *name, bool (*f)()) {
    assert(printf("%s: ", name) > 0);
    assert(fflush(stdout) != EOF);
    const bool ret = f();
    assert(printf(ret ? "ok\n" : "fail\n") > 0);
    return ret;
}

bool assert_fail(bool cond, const char *file, int line, const char *exp) {
    if(!cond)
        fprintf(stderr, "%s:%d: assertion failed: %s\n", file, line, exp);
    return cond;
}

bool assert_eq_fail(
        bool cond, const char *file, int line,
        const char *exp0, const char *exp1) {
    if(!cond)
        fprintf(
            stderr, "%s:%d: assertion failed: %s == %s\n",
            file, line, exp0, exp1);
    return cond;
}

bool assert_ne_fail(
        bool cond, const char *file, int line,
        const char *exp0, const char *exp1) {
    if(!cond)
        fprintf(
            stderr, "%s:%d: assertion failed: %s != %s\n",
            file, line, exp0, exp1);
    return cond;
}

bool check_log(const char *file, int line, const char *expected) {
    FILE *f = log_set(NULL);
    log_set(f);
    assert(fflush(f) != EOF);
    size_t n = ftell(f);
    assert(n > 0);
    assert(fseek(f, 0, SEEK_SET) >= 0);
    char *text = malloc(n);
    fread(text, 1, n, f);
    assert(!ferror(f));
    const bool ret = strncmp(text, expected, strlen(expected) - 1) == 0;
    if(!ret)
        fprintf(stderr, "unexpected log output: %.*s", (int)n, text);
    free(text);
    return ret;
}
