#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tests/common.h"

const char *PROG_NAME = NULL;
const char *CMD_NAME = NULL;

static bool test_log(void) {
    FILE *const tmp = tmpfile();
    log_set(tmp);
    log_err("log\n");
    PROG_NAME = "prog_name";
    log_err("with program name\n");
    CMD_NAME = "cmd_name";
    log_err("with command name\n");
    errno = ENOENT;
    log_errno("with errno %d", ENOENT);
    PROG_NAME = NULL;
    CMD_NAME = NULL;
    return CHECK_LOG(
        "log\n"
        "prog_name: with program name\n"
        "prog_name: cmd_name: with command name\n"
        "prog_name: cmd_name: with errno 2: No such file or directory\n");
}

static bool test_is_prefix() {
    // clang-format off
    const struct {
        const char *prefix, *s;
        bool is_prefix;
    } t[] = {
        {"", "000", true},
        {"000", "", false},
        {"000", "000", true},
        {"000", "00011", true},
        {"000", "00011", true},
        {"000", "0", false},
        {"001", "000", false},
        {"111", "000", false}};
    // clang-format on
    bool ret = true;
    for(size_t i = 0, n = sizeof(t) / sizeof(*t); i < n; ++i) {
        assert(printf("\n  \"%s\" \"%s\" ", t[i].prefix, t[i].s) > 0);
        assert(fflush(stdout) == 0);
        const bool iret = t[i].is_prefix == !!(is_prefix(t[i].prefix, t[i].s));
        if(!iret)
            printf("fail");
        ret &= iret;
    }
    return ret;
}

static bool test_copy_arg_empty() {
    log_set(tmpfile());
    const char *arg = "";
    return ASSERT(!copy_arg("arg_name", NULL, arg, 0))
        && CHECK_LOG("empty arg_name specified\n");
}

static bool test_copy_arg_too_long() {
    log_set(tmpfile());
    const char *arg = "01234567";
    char out[8] = {0};
    return ASSERT(!copy_arg("arg_name", out, arg, 8))
        && ASSERT_STR_EQ_N(out, arg, sizeof(out))
        && CHECK_LOG("arg_name too long (>= 8)\n");
}

static bool test_copy_arg() {
    log_set(tmpfile());
    const char *arg = "arg";
    char out[4];
    return ASSERT(copy_arg("arg_name", out, arg, 4))
        && ASSERT_EQ(strcmp(out, arg), 0);
}

static bool test_exec_err() {
    log_set(tmpfile());
    const char *argv[] = {"", NULL};
    bool ret = ASSERT(!exec(argv, -1, -1));
    char buf[1024];
    snprintf(buf, 1023, "execvp: %s\n", strerror(ENOENT));
    return CHECK_LOG(buf) && ret;
}

static bool test_exec() {
    const char *argv[] = {"true", NULL};
    pid_t pid = fork();
    assert(pid != -1);
    if(!pid)
        exit(!exec(argv, -1, -1));
    return ASSERT(waitpid(-1, NULL, 0));
}

static bool test_exec_output() {
    log_set(stderr);
    const char *argv[] = {"sh", "-c", "echo stdout && exec cat", NULL};
    union {
        int p[4];
        struct {
            int child_read, parent_write, parent_read, child_write;
        };
    } fds;
    static_assert(sizeof(fds) == 4 * sizeof(int), "unexpected padding");
    assert(pipe(fds.p) == 0);
    assert(pipe(fds.p + 2) == 0);
    const pid_t pid = fork();
    assert(pid != -1);
    if(!pid) {
        assert(close(fds.parent_write) == 0);
        assert(close(fds.parent_read) == 0);
        exit(!exec(argv, fds.child_read, fds.child_write));
    }
    assert(close(fds.child_read) == 0);
    assert(close(fds.child_write) == 0);
    const char input[] = "stdin\n";
    const size_t n_write = sizeof(input) - 1;
    assert(n_write < SSIZE_MAX);
    assert(write(fds.parent_write, input, n_write) == (ssize_t)n_write);
    assert(close(fds.parent_write) == 0);
    int status;
    assert(wait(&status) != -1);
    bool ret = ASSERT_EQ(status, 0);
    const char expected[] = "stdout\nstdin\n";
    char output[sizeof(expected)];
    const size_t n_read = sizeof(expected) - 1;
    const ssize_t n = read(fds.parent_read, output, n_read);
    ret = ASSERT_EQ(n, (ssize_t)n_read) && ret;
    ret = ASSERT_STR_EQ_N(output, expected, sizeof(expected) - 1) && ret;
    ret = ASSERT_EQ(read(fds.parent_read, output, n_read), 0) && ret;
    assert(close(fds.parent_read) == 0);
    return ret;
}

static bool test_wait_n_signal() {
    log_set(tmpfile());
    const pid_t pid0 = fork();
    if(!pid0)
        exit(EXIT_FAILURE);
    const pid_t pid1 = fork();
    if(!pid1)
        exit(EXIT_SUCCESS);
    return ASSERT(!wait_n(2)) && CHECK_LOG("child exited: 0\n");
}

static bool test_wait_n_failure() {
    log_set(tmpfile());
    const pid_t pid0 = fork();
    if(!pid0)
        pause();
    const pid_t pid1 = fork();
    if(!pid1)
        exit(EXIT_SUCCESS);
    kill(pid0, SIGTERM);
    bool ret = ASSERT(!wait_n(2));
    char msg[] = "child killed by signal: 00\n";
    snprintf(msg + sizeof(msg) - 4, 4, "%2d\n", SIGTERM);
    return CHECK_LOG(msg) && ret;
}

static bool test_wait_n() {
    log_set(tmpfile());
    const pid_t pid0 = fork();
    if(!pid0)
        exit(EXIT_SUCCESS);
    const pid_t pid1 = fork();
    if(!pid1)
        exit(EXIT_SUCCESS);
    return ASSERT(wait_n(2));
}

static bool test_join_lines() {
    unsigned char input[] = "test\njoin lines\ntest";
    join_lines(input, input + sizeof(input));
    const unsigned char expected[] = "test join lines test";
    if(memcmp(input, expected, sizeof(input)) != 0) {
        fprintf(stderr, "unexpected output: %s\n", input);
        return false;
    }
    return true;
}

static bool test_mtrix_buffer_append() {
    mtrix_buffer b = {.p = NULL, .s = 0};
    mtrix_buffer_append(NULL, 0, 0, &b);
    bool ret = ASSERT_EQ(b.s, 0);
    ret = ASSERT_EQ(b.p, NULL) && ret;
    char input[] = "0123";
    mtrix_buffer_append(input, 2, 1, &b);
    input[2] = 0;
    ret = ASSERT_EQ(b.s, 2) && ret;
    if(memcmp(b.p, input, 3) != 0) {
        fprintf(stderr, "unexpected content: %.*s\n", 2, input);
        ret = false;
    }
    input[2] = '2';
    mtrix_buffer_append(input + 2, 2, 1, &b);
    ret = ASSERT_EQ(b.s, 4) && ret;
    if(memcmp(b.p, input, 5) != 0) {
        fprintf(stderr, "unexpected content: %.*s\n", 2, input);
        ret = false;
    }
    free(b.p);
    return ret;
}

static bool test_build_url_too_long() {
    log_set(tmpfile());
    char buf[MTRIX_MAX_URL_LEN * 2], input[MTRIX_MAX_URL_LEN + 1];
    memset(input, '_', sizeof(input) - 1);
    input[sizeof(input) - 1] = 0;
    const char *parts[] = {input, input, NULL};
    const char expected[] = "url too long (1024 >= 1024):";
    return !build_url(buf, parts)
        && CHECK_LOG_N(expected, sizeof(expected) - 1);
}

static bool test_build_url() {
    log_set(tmpfile());
    char input0[] = "input0", input1[] = "input1";
    char buf[sizeof(input0) + sizeof(input1) - 1];
    const char *parts[] = {input0, input1, NULL};
    bool ret = ASSERT(build_url(buf, parts));
    const char expected[] = "input0input1";
    return ASSERT_STR_EQ_N(buf, expected, sizeof(expected)) && ret;
}

int main() {
    bool ret = true;
    ret = RUN(test_log) && ret;
    ret = RUN(test_is_prefix) && ret;
    ret = RUN(test_copy_arg_empty) && ret;
    ret = RUN(test_copy_arg_too_long) && ret;
    ret = RUN(test_copy_arg) && ret;
    ret = RUN(test_exec_err) && ret;
    ret = RUN(test_exec) && ret;
    ret = RUN(test_exec_output) && ret;
    ret = RUN(test_wait_n_signal) && ret;
    ret = RUN(test_wait_n_failure) && ret;
    ret = RUN(test_wait_n) && ret;
    ret = RUN(test_join_lines) && ret;
    ret = RUN(test_mtrix_buffer_append) && ret;
    ret = RUN(test_build_url_too_long) && ret;
    ret = RUN(test_build_url) && ret;
    return !ret;
}
