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
    return ASSERT(!copy_arg("arg_name", (struct mtrix_buffer){0}, arg))
        && CHECK_LOG("empty arg_name specified\n");
}

static bool test_copy_arg_too_long() {
    log_set(tmpfile());
    const char *arg = "01234567";
    char out[8] = {0};
    struct mtrix_buffer b = {.p = out, .n = 8};
    return ASSERT(!copy_arg("arg_name", b, arg))
        && ASSERT_STR_EQ_N(out, arg, sizeof(out))
        && CHECK_LOG("arg_name too long (>= 8)\n");
}

static bool test_copy_arg() {
    log_set(tmpfile());
    const char *arg = "arg";
    char out[4];
    struct mtrix_buffer b = {.p = out, .n = 4};
    return ASSERT(copy_arg("arg_name", b, arg))
        && ASSERT_EQ(strcmp(out, arg), 0);
}

static bool test_read_write_all(void) {
    log_set(stderr);
    const char check_begin[] = "check begin", check_end[] = "check end";
    const size_t n = 1u << 20;
    const size_t n_begin = sizeof(check_begin) - 1;
    const size_t n_end = sizeof(check_end) - 1;
    const size_t end_off = n - n_end;
    FILE *const tmp = tmpfile();
    assert(tmp);
    char *buffer = malloc(n);
    if(!buffer)
        return false;
    memset(buffer, 0, n);
    memcpy(buffer, check_begin, n_begin);
    memcpy(buffer + end_off, check_end, n_end);
    const int fd = fileno(tmp);
    bool ret = write_all(fd, buffer, n);
    rewind(tmp);
    memset(buffer, 0, n);
    ret = read_all(fd, buffer, n) && ret;
    ret = ASSERT_STR_EQ_N(buffer, check_begin, n_begin) && ret;
    ret = ASSERT_STR_EQ_N(buffer + end_off, check_end, n_end) && ret;
    free(buffer);
    return ret;
}

static bool test_exec_err() {
    log_set(tmpfile());
    const char *argv[] = {"", NULL};
    bool ret = ASSERT(!exec(argv, -1, -1, -1));
    char buf[1024];
    snprintf(buf, 1023, "execvp: %s\n", strerror(ENOENT));
    return CHECK_LOG(buf) && ret;
}

static bool test_exec() {
    const char *argv[] = {"true", NULL};
    pid_t pid = fork();
    assert(pid != -1);
    if(!pid)
        exit(!exec(argv, -1, -1, -1));
    return ASSERT(waitpid(-1, NULL, 0));
}

static bool test_exec_output() {
    log_set(stderr);
    const char *argv[] = {
        "sh", "-c",
        "echo stdout && echo >&2 stderr && exec cat",
        NULL,
    };
    union {
        int p[6];
        struct {
            struct { int read, write; } in, out, err;
        };
    } fds;
    static_assert(sizeof(fds) == 6 * sizeof(int), "unexpected padding");
    assert(pipe(fds.p) == 0);
    assert(pipe(fds.p + 2) == 0);
    assert(pipe(fds.p + 4) == 0);
    const pid_t pid = fork();
    assert(pid != -1);
    if(!pid) {
        assert(close(fds.in.write) == 0);
        assert(close(fds.out.read) == 0);
        assert(close(fds.err.read) == 0);
        exit(!exec(argv, fds.in.read, fds.out.write, fds.err.write));
    }
    assert(close(fds.in.read) == 0);
    assert(close(fds.out.write) == 0);
    assert(close(fds.err.write) == 0);
    const char input[] = "stdin\n";
    const size_t n_write = sizeof(input) - 1;
    assert(n_write < SSIZE_MAX);
    assert(write(fds.in.write, input, n_write) == (ssize_t)n_write);
    assert(close(fds.in.write) == 0);
    int status;
    assert(wait(&status) != -1);
    bool ret = ASSERT_EQ(status, 0);
    const char expected_out[] = "stdout\nstdin\n";
    const char expected_err[] = "stderr\n";
    const size_t out_len = sizeof(expected_out) - 1;
    const size_t err_len = sizeof(expected_err) - 1;
    char output[out_len + err_len];
    {
        const ssize_t n = read(fds.out.read, output, out_len);
        ret = ASSERT_EQ(n, (ssize_t)out_len) && ret;
        ret = ASSERT_EQ(read(fds.out.read, output, out_len), 0) && ret;
        assert(close(fds.out.read) == 0);
    }
    {
        const ssize_t n = read(fds.err.read, output + out_len, err_len);
        ret = ASSERT_EQ(n, (ssize_t)err_len) && ret;
        ret = ASSERT_EQ(read(fds.err.read, output, err_len), 0) && ret;
        assert(close(fds.err.read) == 0);
    }
    ret = ASSERT_STR_EQ_N(output, expected_out, out_len) && ret;
    ret = ASSERT_STR_EQ_N(output + out_len, expected_err, err_len) && ret;
    return ret;
}

static bool test_wait_n_signal() {
    log_set(tmpfile());
    pid_t pids[2] = {0};
    pids[0] = fork();
    if(!pids[0])
        pause();
    pids[1] = fork();
    if(!pids[1])
        exit(EXIT_SUCCESS);
    kill(pids[0], SIGTERM);
    bool ret = ASSERT(!wait_n(2, pids));
    char msg[] = "child killed by signal: 00\n";
    snprintf(msg + sizeof(msg) - 4, 4, "%2d\n", SIGTERM);
    return CHECK_LOG(msg) && ret;
}

static bool test_wait_n_failure() {
    log_set(tmpfile());
    pid_t pids[2] = {0};
    pids[0] = fork();
    if(!pids[0])
        exit(EXIT_FAILURE);
    pids[1] = fork();
    if(!pids[1])
        exit(EXIT_SUCCESS);
    return ASSERT(!wait_n(2, pids)) && CHECK_LOG("child exited: 1\n");
}

static bool test_wait_n() {
    log_set(tmpfile());
    pid_t pids[2] = {0};
    pids[0] = fork();
    if(!pids[0])
        exit(EXIT_SUCCESS);
    pids[1] = fork();
    if(!pids[1])
        exit(EXIT_SUCCESS);
    return ASSERT(wait_n(2, pids));
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
    struct mtrix_buffer b = {0};
    mtrix_buffer_append(NULL, 0, 0, &b);
    bool ret = ASSERT_EQ(b.n, 0);
    ret = ASSERT_EQ(b.p, NULL) && ret;
    char input[] = "0123";
    mtrix_buffer_append(input, 2, 1, &b);
    input[2] = 0;
    ret = ASSERT_EQ(b.n, 2) && ret;
    if(memcmp(b.p, input, 3) != 0) {
        fprintf(stderr, "unexpected content: %.*s\n", 2, input);
        ret = false;
    }
    input[2] = '2';
    mtrix_buffer_append(input + 2, 2, 1, &b);
    ret = ASSERT_EQ(b.n, 4) && ret;
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

static bool test_open_or_create_open(void) {
    log_set(stderr);
    char filename[] = "machinatrix_XXXXXX";
    assert(mkstemp(filename) != -1);
    FILE *const f = open_or_create(filename, "r");
    if(!f)
        return false;
    bool ret = true;
    if(fclose(f))
        log_errno("fclose"), ret = false;
    if(unlink(filename))
        log_errno("unlink"), ret = false;
    return ret;
}

static bool test_open_or_create_create(void) {
    log_set(stderr);
    char filename[] = "machinatrix_XXXXXX";
    assert(mkstemp(filename) != -1);
    if(unlink(filename))
        return log_errno("unlink"), false;
    FILE *const f = open_or_create(filename, "r");
    if(!f)
        return false;
    bool ret = true;
    if(fclose(f))
        log_errno("fclose"), ret = false;
    if(unlink(filename))
        log_errno("unlink"), ret = false;
    return ret;
}

int main() {
    bool ret = true;
    ret = RUN(test_log) && ret;
    ret = RUN(test_is_prefix) && ret;
    ret = RUN(test_copy_arg_empty) && ret;
    ret = RUN(test_copy_arg_too_long) && ret;
    ret = RUN(test_copy_arg) && ret;
    ret = RUN(test_read_write_all) && ret;
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
    ret = RUN(test_open_or_create_open) && ret;
    ret = RUN(test_open_or_create_create) && ret;
    return !ret;
}
