#include "numeraria.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <sqlite3.h>

#include "hash.h"
#include "socket.h"
#include "utils.h"

/** Self-pipe FD for interruptions, written to in the signal handler. */
static int signal_fd = -1;
/** Set to the signal number when one is received. */
static volatile sig_atomic_t interrupted = 0;

const char *PROG_NAME = NULL;
const char *CMD_NAME = NULL;

enum {
    /** `--help` was selected. */
    HELP_FLAG = 1u << 0,
    /** `--verbose` was selected. */
    VERBOSE_FLAG = 1u << 1,
    /** Set when the \c exit command is received. */
    EXIT_FLAG = 1u << 0,
    MAX_PATH = MTRIX_MAX_PATH,
    MAX_UNIX_PATH = MTRIX_MAX_UNIX_PATH,
    MAX_CMD = MTRIX_NUMERARIA_MAX_CMD,
    MAX_PACKET = MTRIX_NUMERARIA_MAX_PACKET,
    CMD_SIZE = MTRIX_NUMERARIA_CMD_SIZE,
    /**
     * Maximum number of open connections.
     * Includes the self-pipe used for signal handling.  May include the `stdin`
     * descriptor if that mode is selected.
     */
    MAX_CONN = 16,
};

/** `numeraria` global configuration. */
struct config {
    /** \see HELP_FLAG, \see VERBOSE_FLAG */
    uint8_t flags;
    /** TCP listening socket, if required. */
    int socket_fd;
    /** Unix listening socket, if required. */
    int unix_fd;
    /** Read end of the self-pipe used for signal notifications. */
    int signal_fd;
    /** Number of client file descriptors in `fds`. */
    size_t n_fds;
    /** Client file descriptor list for `poll(2)`. */
    struct pollfd fds[MAX_CONN];
    /** Database connection. */
    sqlite3 *sqlite;
    /** Fields read from the command line. */
    struct {
        uint8_t flags;
        /** SQLite database path (`:memory:` is used if not set). */
        char db_path[MAX_PATH];
        /** TCP socket listening address. */
        char socket_path[MAX_PATH];
        /** Unix socket listening path. */
        char unix_path[MAX_UNIX_PATH];
    } input;
};

/** Program entry point. */
int main(int argc, const char *const *argv);

/** Parses command-line arguments and fills `config->input`. */
static bool parse_args(int *argc, char *const **argv, struct config *config);

/** Prints a usage message. */
static void usage(FILE *f);

/** Initializes values in `config`. */
static void config_init(struct config *config);

/** Destructs `config`. */
static bool config_destroy(struct config *config);

/** Logs a message if verbose output was requested. */
static void config_verbose(const struct config *config, const char *fmt, ...);

/** Indicates whether all requested input has been processed. */
static bool config_done(const struct config *config);

/** Initializes `config` after the inputs have been determined. */
static bool config_setup(struct config *config);

/** Initializes the self-pipe used for signal notification. */
static bool config_setup_self_pipe(struct config *config);

/** Processes all input files. */
static bool config_input(struct config *config);

/** Accepts new connections from all configured sockets. */
static bool config_accept(struct config *config, int fd);

/** Removes an input file descriptor when it's done. */
static bool config_remove_fd(struct config *config, size_t i);

/** Reads and processes a command from the file descriptor with index `i`. */
static bool config_process_cmd(
    struct config *config, size_t i, uint32_t len, uint8_t cmd);

/** Processes the raw SQL command. */
static bool config_process_sql(
    const struct config *config, int fd, const char *sql, size_t len);

/** Processes the record command command. */
static bool config_record_command(
    const struct config *config, int fd, const char *cmd, size_t len);

/** Signal handler. */
static void handle_signal(int s);

/** Sets up a listening socket for a given address. */
static bool setup_socket(
    int fd, const struct sockaddr *addr, socklen_t addr_len);

/** Initializes the SQLite database. */
static bool setup_db(const char *path, sqlite3 **sqlite);

/** SQLite error callback which prints error messages. */
static void sqlite_error_callback(void *data, int code, const char *msg);

/** Reads a command header from a file descriptor. */
static bool read_cmd(int fd, struct mtrix_numeraria_cmd *cmd);

int main(int argc, const char *const *argv) {
    log_set(stderr);
    PROG_NAME = argv[0];
    struct config config = {0};
    config_init(&config);
    if(!parse_args(&argc, (char *const **)&argv, &config))
        return 1;
    if(config.input.flags & HELP_FLAG)
        return usage(stdout), 0;
    if(config.input.flags & VERBOSE_FLAG)
        config.flags |= VERBOSE_FLAG;
    int ret = 1;
    if(!config_setup(&config))
        goto end;
    while(!(interrupted || config_done(&config)))
        if(!config_input(&config))
            goto end;
    ret = 0;
end:
    if(!config_destroy(&config))
        return 1;
    if(interrupted) {
        printf("\n");
        return 128 + (int)interrupted;
    }
    return ret;
}

bool parse_args(int *argc, char *const **argv, struct config *config) {
    enum { DB_PATH, BIND };
    static const char *short_opts = "hv";
    static const struct option long_opts[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"db-path", required_argument, 0, DB_PATH},
        {"bind", required_argument, 0, BIND},
        {0},
    };
    for(;;) {
        int idx = 0;
        const int c = getopt_long(*argc, *argv, short_opts, long_opts, &idx);
        if(c == -1)
            break;
        switch(c) {
        case 'h': config->input.flags |= HELP_FLAG; continue;
        case 'v': config->input.flags |= VERBOSE_FLAG; continue;
        case DB_PATH: {
            struct mtrix_buffer b = {.p = config->input.db_path, .n = MAX_PATH};
            if(!copy_arg("db path", b, optarg))
                return false;
            break;
        }
        case BIND: {
            const char prefix[] = "unix:";
            char *src = optarg, *dst = config->input.socket_path;
            if(strncmp(src, prefix, sizeof(prefix) - 1) == 0) {
                src += sizeof(prefix) - 1;
                dst = config->input.unix_path;
            }
            struct mtrix_buffer b = {.p = dst, .n = MAX_PATH};
            if(!copy_arg("bind", b, src))
                return false;
            break;
        }
        default: return false;
        }
    }
    if(!*config->input.db_path)
        strcpy(config->input.db_path, ":memory:");
    *argv += optind;
    *argc -= optind;
    return true;
}

void usage(FILE *f) {
    fprintf(
        f,
        "usage: %s --db-path path [options]\n"
        "\n"
        "Options:\n"
        "    -h, --help             this help\n"
        "    -v, --verbose          verbose output\n"
        "    --db-path              path to the database file (`:memory:` is\n"
        "                           used if not set)\n"
        "    --bind-socket addr     bind socket to `addr`, prefix path with\n"
        "                           `unix:` to use a Unix domain socket\n"
        "\n"
        "If no --bind* argument is given, commands are read from stdin.\n",
        PROG_NAME);
}

void config_init(struct config *config) {
    config->socket_fd = -1;
    config->unix_fd = -1;
    for(int i = 0; i < MAX_CONN; ++i)
        config->fds[i] = (struct pollfd){.fd = -1, .events = POLLIN};
}

bool config_destroy(struct config *config) {
    bool ret = true;
    if(config->unix_fd != -1 && unlink(config->input.unix_path) == -1) {
        log_errno("failed to unlink unix socket %s", config->input.unix_path);
        ret = false;
    }
    for(size_t i = 0; i < config->n_fds; ++i)
        if(config->fds[i].fd != -1 && close(config->fds[i].fd) == -1) {
            log_errno("failed to close client fd");
            ret = false;
        }
    sqlite3_close(config->sqlite);
    return ret;
}

void config_verbose(const struct config *config, const char *fmt, ...) {
    if(!(config->flags & VERBOSE_FLAG))
        return;
    va_list argp;
    va_start(argp, fmt);
    log_errv(fmt, argp);
    va_end(argp);
}

bool config_done(const struct config *config) {
    return config->flags & EXIT_FLAG || (
        config->socket_fd == -1
        && config->unix_fd == -1
        && (config->n_fds == 1 && config->fds[0].fd == 0));
}

bool config_setup(struct config *config) {
    sqlite3_config(SQLITE_CONFIG_LOG, sqlite_error_callback, NULL);
    if(!setup_db(config->input.db_path, &config->sqlite))
        return false;
    const char *const socket_path = config->input.socket_path;
    const char *const unix_path = config->input.unix_path;
    if(*socket_path) {
        struct addrinfo *const addr = mtrix_socket_addr(socket_path);
        if(!addr) {
            log_err("%s: failed to determine address\n", __func__);
            return false;
        }
        const int fd = socket(AF_INET, SOCK_STREAM, 0);
        if(fd == -1) {
            log_errno("failed to create TCP socket");
            freeaddrinfo(addr);
            return false;
        }
        const bool ret = setup_socket(fd, addr->ai_addr, addr->ai_addrlen);
        freeaddrinfo(addr);
        if(!ret)
            return false;
        config->fds[config->n_fds++].fd = config->socket_fd = fd;
    }
    if(*unix_path) {
        const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if(fd == -1)
            return log_errno("failed to create unix socket"), false;
        struct sockaddr_un addr = {0};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, unix_path, MAX_UNIX_PATH);
        addr.sun_path[MAX_UNIX_PATH - 1] = 0;
        if(!setup_socket(fd, (const struct sockaddr*)&addr, sizeof(addr)))
            return false;
        config->fds[config->n_fds++].fd = config->unix_fd = fd;
    }
    if(!(*socket_path || *unix_path))
        config->fds[config->n_fds++].fd = STDIN_FILENO;
    if(!config_setup_self_pipe(config))
        return false;
    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);
    return true;
}

bool config_setup_self_pipe(struct config *config) {
    int fds[2];
    if(pipe(fds) == -1)
        return log_errno("pipe"), false;
    if(fcntl(fds[0], F_SETFL, O_NONBLOCK) == -1) {
        log_errno("failed to make pipe non-blocking");
        goto err;
    }
    if(fcntl(fds[1], F_SETFL, O_NONBLOCK) == -1) {
        log_errno("failed to make pipe non-blocking");
        goto err;
    }
    signal_fd = fds[1];
    config->fds[config->n_fds++].fd = config->signal_fd = fds[0];
    return true;
err:
    if(close(fds[0]) == -1)
        log_errno("failed to close pipe");
    if(close(fds[1]) == -1)
        log_errno("failed to close pipe");
    return false;
}

bool config_input(struct config *config) {
    const int signal_fd = config->signal_fd;
    const int max_socket = config->socket_fd > config->unix_fd
        ? config->socket_fd : config->unix_fd;
    struct pollfd *const fds = config->fds;
    int n = poll(config->fds, config->n_fds, -1);
    config_verbose(config, "poll: %d update(s)\n", n);
    if(n == -1) {
        if(errno == EINTR)
            return true;
        return log_errno("poll"), false;
    }
    for(size_t i = 0; n;) {
        assert(i < config->n_fds);
        const short revents = fds[i].revents;
        if(!revents) {
            ++i;
            continue;
        }
        --n;
        fds[i].revents = 0;
        const int fd = fds[i].fd;
        config_verbose(config, "poll: fd %d revents: 0x%x\n", fd, revents);
        assert(fd != -1);
        if(revents & POLLERR) {
            log_err("poll: got POLLERR for fd %d\n", fd);
            config_remove_fd(config, i);
            return false;
        }
        if(revents & POLLHUP && !(revents & POLLIN)) {
            if(!config_remove_fd(config, i))
                return false;
            continue;
        }
        assert(revents & POLLIN);
        if(fd == signal_fd)
            return config_verbose(config, "signal received\n"), true;
        if(fd <= max_socket) {
            if(!config_accept(config, fd))
                return false;
            ++i;
            continue;
        }
        struct mtrix_numeraria_cmd cmd = {0};
        if(!read_cmd(fd, &cmd))
            return false;
        if(!cmd.cmd) {
            config_verbose(config, "fd %d done\n", fd);
            if(!config_remove_fd(config, i))
                return false;
            continue;
        }
        if(!config_process_cmd(config, i, cmd.len, cmd.cmd))
            return false;
        ++i;
    }
    return true;
}

bool config_accept(struct config *config, int fd) {
    struct pollfd *out = config->fds + config->n_fds;
    for(size_t n = config->n_fds; n < MAX_CONN;) {
        const int new_fd = accept(fd, NULL, NULL);
        if(new_fd == -1) {
            if(errno == EWOULDBLOCK)
                break;
            return log_err("failed to accept unix socket connection"), false;
        }
        config->n_fds = ++n;
        (out++)->fd = new_fd;
    }
    return true;
}

bool config_remove_fd(struct config *config, size_t i) {
    assert(i < config->n_fds);
    const int fd = config->fds[i].fd;
    --config->n_fds;
    config->fds[i] = config->fds[config->n_fds];
    config->fds[config->n_fds].fd = -1;
    config->fds[config->n_fds].revents = 0;
    if(close(fd) == -1)
        return log_errno("failed to close client fd"), false;
    return true;
}

bool config_process_cmd(
    struct config *config, size_t i, uint32_t len, uint8_t cmd
) {
    config_verbose(config, "%s: cmd 0x%x, len %u\n", __func__, cmd, len);
    const int fd = config->fds[i].fd;
    switch(cmd) {
    case MTRIX_NUMERARIA_CMD_EXIT: config->flags |= EXIT_FLAG; return true;
    case MTRIX_NUMERARIA_CMD_SQL: {
        char buffer[MAX_CMD];
        if(!read_all(fd, buffer, len))
            return log_err("%s: failed to read command\n", __func__), false;
        return config_process_sql(config, fd, buffer, len);
    }
    case MTRIX_NUMERARIA_CMD_RECORD_CMD: {
        char buffer[MAX_CMD];
        if(!read_all(fd, buffer, len))
            return log_err("%s: failed to read command\n", __func__), false;
        return config_record_command(config, fd, buffer, len);
    }
    case MTRIX_NUMERARIA_CMD_STATS: {
        const char cmd[] =
            "select count, cmd, arg0 from machinatrix_stats_cmd"
            " order by count desc;";
        return config_process_sql(config, fd, cmd, sizeof(cmd) - 1);
    }
    default: return log_err("invalid command: %d\n", cmd), false;
    }
}

bool config_process_sql(
    const struct config *config, int fd, const char *sql, size_t len
) {
    config_verbose(config, "%s: %.*s\n", __func__, (int)len, sql);
    sqlite3_stmt *stmt = NULL;
    sqlite3_prepare_v3(config->sqlite, sql, (int)len, 0, &stmt, NULL);
    if(!stmt)
        return false;
    bool ret = false;
    for(;;) {
        switch(sqlite3_step(stmt)) {
        case SQLITE_ROW: break;
        case SQLITE_BUSY: continue;
        case SQLITE_DONE: goto done;
        default: goto end;
        }
        const size_t n = (size_t)sqlite3_column_count(stmt);
        config_verbose(config, "row with %zu columns\n", n);
        if(write(fd, &n, sizeof(n)) != sizeof(n)) {
            log_errno("write");
            goto end;
        }
        for(size_t i = 0; i < n; ++i) {
            const size_t text_len = (size_t)sqlite3_column_bytes(stmt, (int)i);
            const unsigned char *const text = sqlite3_column_text(stmt, (int)i);
            config_verbose(
                config, "column %zu (len: %zu): %.*s\n",
                i, text_len, text_len, text);
            if(write(fd, &text_len, sizeof(text_len)) != sizeof(text_len)) {
                log_errno("write");
                goto end;
            }
            if(!text_len)
                continue;
            if(write(fd, text, text_len) != (ssize_t)text_len) {
                log_errno("write");
                goto end;
            }
        }
    }
done:
    config_verbose(config, "done\n");
    const size_t zero = 0;
    if(write(fd, &zero, sizeof(zero)) != sizeof(zero)) {
        log_errno("write");
        goto end;
    }
    ret = true;
end:
    config_verbose(config, "finalize\n");
    if(sqlite3_finalize(stmt) != SQLITE_OK)
        ret = false;
    return ret;
}

bool config_record_command(
    const struct config *config, int fd, const char *cmd, size_t len
) {
    config_verbose(config, "config_record_command: %zu %.*s\n", len, len, cmd);
    const mtrix_hash h = mtrix_hasher_add_bytes(MTRIX_HASHER_INIT, cmd, len).h;
    const char sql[] =
        "insert into machinatrix_stats_cmd"
        " (hash, cmd, arg0, arg1, count) values (?, ?, ?, ?, 1)"
        " on conflict(hash) do update set count = count + 1;";
    static_assert(MTRIX_MAX_ARGS == 2);
    sqlite3_stmt *stmt = NULL;
    sqlite3_prepare_v3(config->sqlite, sql, sizeof(sql) - 1, 0, &stmt, NULL);
    if(!stmt)
        return false;
    bool ret = false;
    if(sqlite3_bind_int64(stmt, 1, (sqlite3_int64)h) != SQLITE_OK)
        goto end;
    size_t n;
    memcpy(&n, cmd, sizeof(n));
    cmd += sizeof(n);
    config_verbose(config, "%zu arguments\n", n);
    for(size_t i = 0; i < n; ++i) {
        size_t len;
        memcpy(&len, cmd, sizeof(len));
        cmd += sizeof(len);
        config_verbose(config, "command %zu: %.*s\n", i, len, cmd);
        if(sqlite3_bind_text(stmt, (int)i + 2, cmd, (int)len, NULL)
                != SQLITE_OK)
            goto end;
        cmd += len;
    }
    if(sqlite3_step(stmt) != SQLITE_DONE)
        goto end;
    const char zero = 0;
    if(!write_all(fd, &zero, sizeof(zero)))
        goto end;
    ret = true;
end:
    if(sqlite3_finalize(stmt) != SQLITE_OK)
        ret = false;
    return ret;
}

void handle_signal(int s) {
    interrupted = s;
    const int e = errno;
    if(write(signal_fd, "", 1) != 1)
        log_errno("handle_signal: write");
    errno = e;
}

bool setup_socket(
    int fd, const struct sockaddr *addr, socklen_t addr_len
) {
    if(bind(fd, addr, addr_len) == -1) {
        log_errno("%s: bind", __func__);
        goto err;
    }
    if(listen(fd, MAX_CONN) == -1) {
        log_errno("%s: listen", __func__);
        goto err;
    }
    if(fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        log_errno("%s: fcntl(F_SETFL, O_NONBLOCK)", __func__);
        goto err;
    }
    return true;
err:
    if(close(fd) == -1)
        log_errno("%s: close", __func__);
    return false;
}

static bool setup_db(const char *path, sqlite3 **sqlite) {
    sqlite3 *p = NULL;
    if(sqlite3_open(path, &p) != 0)
        return false;
    const char *const tables =
        "create table if not exists machinatrix_stats_cmd ("
            "hash int primary key,"
            " cmd text,"
            " arg0 text,"
            " arg1 text,"
            " count int"
        ");";
    if(sqlite3_exec(p, tables, NULL, NULL, NULL) != SQLITE_OK)
        return false;
    *sqlite = p;
    return true;
}

void sqlite_error_callback(void *data, int code, const char *msg) {
    (void)data;
    log_err("sqlite: error code %d: %s\n", code, msg);
}

bool read_cmd(int fd, struct mtrix_numeraria_cmd *cmd) {
    size_t n = CMD_SIZE;
    for(char *p = (void*)cmd; n;) {
        ssize_t nr;
        switch(nr = read(fd, p, n)) {
        default: p += nr, n -= (size_t)nr; break;
        case -1: return log_errno("%s: read", __func__), false;
        case 0:
            if(n == CMD_SIZE)
                return true;
            log_err("%s: invalid read size %d\n", __func__, n);
            return false;
        }
    }
    const size_t len = cmd->len, max = MAX_CMD;
    if(len > max) {
        log_err("%s: invalid length (%zu > %zu)\n", __func__, len, max);
        return false;
    }
    return true;
}
