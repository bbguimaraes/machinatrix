#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/wait.h>
#include <unistd.h>
#include "config.h"
#include "util.h"

const char *PROG_NAME = 0, *CMD_NAME = 0;
#define MAX_ARGS ((size_t)1)
#define DICT_FILE "/usr/share/dict/words"

typedef bool (*mtrix_cmd_f)(const mtrix_config*, int, const char *const*);
typedef struct {
    const char *name;
    mtrix_cmd_f f;
} mtrix_cmd;

int main(int argc, char *const *argv);
bool parse_args(int *argc, char *const **argv, mtrix_config *config);
void usage(FILE *f);
bool handle_cmd(const mtrix_config *config, int argc, const char *const *argv);
bool handle_stdin(const mtrix_config *config);
void str_to_args(char *str, size_t max_args, int *argc, char **argv);
bool cmd_help(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_ping(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_word(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_abbr(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_damn(const mtrix_config *config, int argc, const char *const *argv);

mtrix_cmd COMMANDS[] = {
    {"help", cmd_help},
    {"ping", cmd_ping},
    {"word", cmd_word},
    {"abbr", cmd_abbr},
    {"damn", cmd_damn},
    {0, 0},
};

int main(int argc, char *const *argv) {
    PROG_NAME = argv[0];
    mtrix_config config;
    init_config(&config);
    if(!parse_args(&argc, &argv, &config))
        return 1;
    if(config.help) {
        usage(stdout);
        return 0;
    }
    return argc
        ? !handle_cmd(&config, argc, (const char * const*)argv)
        : !handle_stdin(&config);
}

bool parse_args(int *argc, char *const **argv, mtrix_config *config) {
    static const char *short_opts = "hvn";
    static const struct option long_opts[] = {
        {"help",    no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"dry-run", no_argument, 0, 'n'},
        {0, 0, 0, 0},
    };
    for(;;) {
        int idx = 0;
        const int c = getopt_long(*argc, *argv, short_opts, long_opts, &idx);
        if(c == -1)
            break;
        switch(c) {
        case 'h': config->help = true; continue;
        case 'v': config->verbose = true; continue;
        case 'n': config->dry = true; continue;
        default: return false;
        }
    }
    *argv += optind;
    *argc -= optind;
    return true;
}

void usage(FILE *f) {
    fprintf(f,
        "usage: %s [options] [<cmd>]\n\n"
        "Options:\n"
        "    -h, --help             this help\n"
        "    -v, --verbose          verbose output\n"
        "Commands:\n"
        "    help:                  this help\n"
        "    ping:                  pong\n"
        "    word:                  random word\n"
        "    damn:                  random curse\n"
        "    abbr <acronym> [<dictionary>]:\n"
        "                           random de-abbreviation\n",
        PROG_NAME);
}

bool handle_cmd(const mtrix_config *config, int argc, const char *const *argv) {
    const char *name = *argv++;
    --argc;
    for(mtrix_cmd *cmd = COMMANDS; cmd->name; ++cmd)
        if(!strcmp(name, cmd->name)) {
            CMD_NAME = name;
            return cmd->f(config, argc, argv);
        }
    log_err("unknown command: %s\n", name);
    usage(stderr);
    return false;
}

bool handle_stdin(const mtrix_config *config) {
    bool ret = true; char *buffer = 0;
    for(;;) {
        size_t len;
        if((len = getline(&buffer, &len, stdin)) == -1)
            break;
        int argc; char *argv[MAX_ARGS];
        str_to_args(buffer, MAX_ARGS, &argc, argv);
        if(!argc)
            continue;
        mtrix_cmd *cmd = COMMANDS;
        while(cmd->name && strcmp(argv[0], cmd->name))
            ++cmd;
        if(!cmd->name) {
            log_err("unknown command: %s\n", argv[0]);
            ret = false;
            break;
        }
        CMD_NAME = cmd->name;
        ret = cmd->f(config, argc - 1, (const char *const*)argv + 1);
        CMD_NAME = 0;
        if(!ret)
            break;
    }
    if(buffer)
        free(buffer);
    return ret;
}

void str_to_args(char *str, size_t max_args, int *argc, char **argv) {
    const char *d = " \n";
    *argc = 0;
    char *saveptr, *arg = strtok_r(str, d, &saveptr);
    if(arg)
        do argv[(*argc)++] = arg;
        while(*argc < max_args && (arg = strtok_r(0, d, &saveptr)));
}

bool cmd_help(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc) {
        log_err("wrong number of arguments (%i, want 0)\n", argc);
        return false;
    }
    usage(stdout);
    return true;
}

bool cmd_ping(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc) {
        log_err("wrong number of arguments (%i, want 0)\n", argc);
        return false;
    }
    printf("pong\n");
    return true;
}

bool cmd_word(const mtrix_config *config, int argc, const char *const *_) {
    if(argc) {
        log_err("wrong number of arguments (%i, want 0)\n", argc);
        return false;
    }
    pid_t pid = fork();
    if(pid == -1) {
        log_err("fork: %s\n", strerror(errno));
        return false;
    }
    if(!pid) {
        static const char *argv[] = {"shuf", "-n", "1", DICT_FILE, 0};
        return exec(argv, 0, 0);
    }
    return wait_n(1);
}

bool cmd_abbr(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc != 1) {
        log_err("wrong number of arguments (%i, want 1)\n", argc);
        return false;
    }
    for(const char *c = argv[0]; *c; ++c) {
        int fds[2][2];
        if(pipe(fds[0]) == -1) {
            log_err("pipe: %s\n", strerror(errno));
            return false;
        }
        pid_t pid0 = fork();
        if(pid0 == -1) {
            log_err("fork: %s\n", strerror(errno));
            return false;
        }
        if(!pid0) {
            close(fds[0][0]);
            const char *cargv[] = {"look", 0, 0};
            char arg[] = {*c, '\0'};
            cargv[1] = arg;
            return exec(cargv, 0, fds[0][1]);
        }
        close(fds[0][1]);
        if(pipe(fds[1]) == -1) {
            log_err("pipe: %s\n", strerror(errno));
            return false;
        }
        pid_t pid1 = fork();
        if(!pid1) {
            close(fds[1][0]);
            const char *cargv[] = {"shuf", "-n", "1", 0};
            return exec(cargv, fds[0][0], fds[1][1]);
        }
        close(fds[0][0]);
        close(fds[1][1]);
        FILE *child = fdopen(fds[1][0], "r");
        if(!child) {
            log_err("fdopen: %s\n", strerror(errno));
            return false;
        }
        char *buffer = 0;
        size_t len;
        if((len = getline(&buffer, &len, child)) == -1)
            break;
        if(buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';
        printf("%s%s", c != argv[0] ? " " : "", buffer);
        if(!wait_n(2))
            return false;
    }
    printf("\n");
    return true;
}

bool cmd_damn(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc > 1) {
        log_err("wrong number of arguments (%i, max 1)\n", argc);
        return false;
    }
    int fds[2];
    if(pipe(fds) == -1) {
        log_err("pipe: %s\n", strerror(errno));
        return false;
    }
    pid_t pid = fork();
    if(pid == -1) {
        log_err("fork: %s\n", strerror(errno));
        return false;
    }
    if(!pid) {
        close(fds[0]);
        const char *cargv[] =
            {"shuf", DICT_FILE, "-n", argc ? argv[0] : "3", 0};
        return exec(cargv, 0, fds[1]);
    }
    close(fds[1]);
    FILE *child = fdopen(fds[0], "r");
    if(!child) {
        log_err("fdopen: %s\n", strerror(errno));
        return false;
    }
    printf("You");
    char *buffer = 0;
    for(;;) {
        size_t len;
        if((len = getline(&buffer, &len, child)) == -1)
            break;
        if(buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';
        printf(" %s", buffer);
    }
    printf("!\n");
    if(buffer)
        free(buffer);
    return wait_n(1);
}
