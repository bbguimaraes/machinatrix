/**
 * \file
 * Main robot implementation.
 */
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <fcntl.h>
#include <getopt.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"

#include <tidybuffio.h>

#include "config.h"
#include "dlpo.h"
#include "hash.h"
#include "html.h"
#include "numeraria.h"
#include "utils.h"
#include "wikt.h"

#define ARRAY_SIZE(v) (sizeof(v) / sizeof(*v))
#define RND_LIST_ITEM(v) (rnd_list_item((v), ARRAY_SIZE(v)))

enum {
    MAX_PATH = MTRIX_MAX_PATH,
    MAX_UNIX_PATH = MTRIX_MAX_UNIX_PATH,
    STATS_WIKT = 0,
    STATS_DLPO = 1,
    /** Whether a command requires a random number generator. */
    NEEDS_RNG = 1 << 0,
    /** Whether the random number generator has been initialized. */
    RND_INITIALIZED = 1 << 0,
};

/** `machinatrix`-specific configuration. */
struct config {
    /** Shared configuration fields. */
    struct mtrix_config c;
    /** Socket to communicate with `numeraria`, if enabled. */
    int numeraria_fd;
    /** Runtime flags. */
    uint8_t flags;
    /** Fields read from the command line. */
    struct {
        /** Optional path to the statistics file. */
        char stats_file[MAX_PATH];
        /** Optional address of the numeraria server socket. */
        char numeraria_socket[MAX_PATH];
        /** Optional path of the numeraria server Unix socket. */
        char numeraria_unix[MAX_UNIX_PATH];
    } input;
};

/** To be filled by `argv[0]` later, for logging. */
const char *PROG_NAME = NULL;

/** Set while processing a command. */
const char *CMD_NAME = NULL;

/** Dictionary file used for commands that require a list of words. */
#define DICT_FILE "/usr/share/dict/words"

/** Function that handles a command. */
typedef bool mtrix_cmd_f(const struct config*, const char *const*);

/** Structure that associates a \ref mtrix_cmd_f to a command. */
struct mtrix_cmd {
    /** Hash of field `name`. */
    uint64_t name_hash;
    /** Command name. */
    const char *name;
    /** Function that handles the command. */
    mtrix_cmd_f *f;
    /** Configuration flags. */
    uint8_t flags;
};

/** Accumulates lookup statistics. */
struct stats {
    /** Number of Wiktionary lookups. */
    int wikt;
    /** Number of DLPO lookups. */
    int dlpo;
};

/** Program entry point. */
int main(int argc, const char *const *argv);

/** Perform global initialization. */
static void init(void);

/** Initializes the random number generator. */
static void rnd_init(struct config *config);

/** Parses command-line arguments and fills `config`. */
static bool parse_args(int argc, char *const **argv, struct config *config);

/** Prints a usage message. */
static void usage(FILE *f);

/** Initializes values in `config`. */
static void config_init(struct config *config);

/** Initializes values in `config`. */
static bool config_init_numeraria(struct config *config);

/** Destructs `config`. */
static bool config_destroy(struct config *config);

/** Records that a command will be executed, along with its arguments. */
static bool config_record_command(
    const struct config *config, const char *const *argv);

/** Handles a command passed via the command line. */
static bool handle_cmd(struct config *config, const char *const *argv);

/** Handles commands read as lines from a file. */
static bool handle_file(struct config *config, FILE *f);

/**
 * Breaks string into space-separated parts.
 * \param str Input string, modified to include null terminators.
 * \param max_args Maximum length for `argv`, including the null terminator.
 * \param argv Populated with pointers to the null-terminated strings in `str`.
 * \return \c false if \c str contains more than \c max_args
 */
static bool str_to_args(char *str, size_t max_args, char **argv);

/** Chooses an item randomly from a list of strings. */
static const char *rnd_list_item(const char **v, size_t n);

/** Parses \p i as an index (1-based) into \p v. */
static const char *list_item(const char **v, size_t n, const char *i);

/** Implements the `help` command. */
static bool cmd_help(const struct config *config, const char *const *argv);

/** Implements the `ping` command. */
static bool cmd_ping(const struct config *config, const char *const *argv);

/** Implements the `word` command. */
static bool cmd_word(const struct config *config, const char *const *argv);

/** Implements the `abbr` command. */
static bool cmd_abbr(const struct config *config, const char *const *argv);

/** Implements the `damn` command. */
static bool cmd_damn(const struct config *config, const char *const *argv);

/** Implements the `parl` command. */
static bool cmd_parl(const struct config *config, const char *const *argv);

/** Implements the `bard` command. */
static bool cmd_bard(const struct config *config, const char *const *argv);

/** Implements the `dlpo` command. */
static bool cmd_dlpo(const struct config *config, const char *const *argv);

/** Implements the `wikt` command. */
static bool cmd_wikt(const struct config *config, const char *const *argv);

/** Implements the `tr` command. */
static bool cmd_tr(const struct config *config, const char *const *argv);

/** Common implementation of AoE commands. */
static bool cmd_aoe(const char **v, size_t n, const char *const *argv);

/** Implements the `aoe1` command. */
static bool cmd_aoe1(const struct config *config, const char *const *argv);

/** Implements the `aoe2` command. */
static bool cmd_aoe2(const struct config *config, const char *const *argv);

/** Implements the `stats` command. */
static bool cmd_stats(const struct config *config, const char *const *argv);

/** Increments the count on wikt and dlpo lookups. */
static bool stats_increment(const struct config *config, uint8_t opt);

/** Print stats from local file. */
static bool stats_file(const struct config *config);

/** Print stats from numeraria. */
static bool stats_numeraria(const struct config *config);

/**
 * Maps a command name to the function that handles it.
 * Terminated by a `{NULL, NULL}` entry.
 */
const struct mtrix_cmd COMMANDS[] = {
    {0x00005979ab, "tr",    cmd_tr,    0},
    {0x017c93ee3c, "abbr",  cmd_abbr,  0},
    {0x017c9425ab, "aoe1",  cmd_aoe1,  NEEDS_RNG},
    {0x017c9425ac, "aoe2",  cmd_aoe2,  NEEDS_RNG},
    {0x017c94785e, "bard",  cmd_bard,  NEEDS_RNG},
    {0x017c959085, "damn",  cmd_damn,  0},
    {0x017c95bfb4, "dlpo",  cmd_dlpo,  0},
    {0x017c97d2ee, "help",  cmd_help,  0},
    {0x017c9c25b4, "parl",  cmd_parl,  NEEDS_RNG},
    {0x017c9c4733, "ping",  cmd_ping,  0},
    {0x017ca01d84, "wikt",  cmd_wikt,  0},
    {0x017ca037e1, "word",  cmd_word,  0},
    {0x3110614a14, "stats", cmd_stats, 0},
    {0},
};

int main(int argc, const char *const *argv) {
    init();
    PROG_NAME = argv[0];
    struct config config = {0};
    config_init(&config);
    if(!parse_args(argc, (char *const **)&argv, &config))
        return 1;
    if(config.c.flags & MTRIX_CONFIG_HELP)
        return usage(stdout), 0;
    return !(
        config_init_numeraria(&config)
        && (*argv ? handle_cmd(&config, argv) : handle_file(&config, stdin))
        && config_destroy(&config));
}

void init(void) {
    log_set(stderr);
#ifndef NDEBUG
    for(const struct mtrix_cmd *p = COMMANDS; p->name; ++p)
        assert(mtrix_hash_str(p->name) == p->name_hash);
    for(const struct mtrix_cmd *p = COMMANDS + 1; p->name; ++p)
        assert(p[-1].name_hash < p->name_hash);
#endif
}

void rnd_init(struct config *config) {
    srand((unsigned)time(NULL));
    config->flags |= RND_INITIALIZED;
}

bool parse_args(int argc, char *const **argv, struct config *config) {
    enum { STATS_FILE, NUMERARIA_SOCKET };
    static const char *short_opts = "hvn";
    static const struct option long_opts[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"dry-run", no_argument, 0, 'n'},
        {"stats-file", required_argument, 0, STATS_FILE},
        {"numeraria-socket", required_argument, 0, NUMERARIA_SOCKET},
        {0, 0, 0, 0},
    };
    for(;;) {
        int idx = 0;
        const int c = getopt_long(argc, *argv, short_opts, long_opts, &idx);
        if(c == -1)
            break;
        switch(c) {
        case 'h': config->c.flags |= MTRIX_CONFIG_HELP; continue;
        case 'v': config->c.flags |= MTRIX_CONFIG_VERBOSE; continue;
        case 'n': config->c.flags |= MTRIX_CONFIG_DRY; continue;
        case STATS_FILE: {
            struct mtrix_buffer b = {
                .p = config->input.stats_file,
                .n = MAX_PATH,
            };
            if(!copy_arg("stats file", b, optarg))
                return false;
            break;
        }
        case NUMERARIA_SOCKET: {
            const char prefix[] = "unix:";
            char *src = optarg, *dst = config->input.numeraria_socket;
            if(strncmp(src, prefix, sizeof(prefix) - 1) == 0) {
                src += sizeof(prefix) - 1;
                dst = config->input.numeraria_unix;
            }
            struct mtrix_buffer b = {.p = dst, .n = MAX_PATH};
            if(!copy_arg("numeraria socket", b, src))
                return false;
            break;
        }
        default: return false;
        }
    }
    if(*config->input.numeraria_socket && *config->input.numeraria_unix) {
        log_err(
            "--numeraria-socket and --numeraria-unix are mutually exclusive");
        return false;
    }
    *argv += optind;
    return true;
}

void usage(FILE *f) {
    fprintf(
        f,
        "usage: %s [options] [<cmd>]\n\n"
        "Options:\n"
        "    -h, --help             this help\n"
        "    -v, --verbose          verbose output\n"
        "    -n, --dry-run          don't access external services\n"
        "    --stats-file path      path to file where stats are stored\n"
        "    --numeraria-socket address\n"
        "                           address to connect to numeraria\n"
        "    --numeraria-unix path  path to connect to numeraria\n"
        "\n"
        "Commands:\n"
        "    help:                  this help\n"
        "    ping:                  pong\n"
        "    word:                  random word\n"
        "    damn:                  random curse\n"
        "    abbr <acronym> [<dictionary>]:\n"
        "                           random de-abbreviation\n"
        "    bard:                  random Shakespearean insult\n"
        "    dlpo <term>:           lookup etymology (DLPO)\n"
        "    wikt <term> [<lang>]:  lookup etymology (Wiktionary)\n"
        "    parl:                  use unparliamentary language\n"
        "    tr <term> [<lang>]:    lookup translation (Wiktionary)\n"
        "    aoe1|aoe2 [<n>]        Age of Empires I/II taunt\n"
        "    stats:                 print statistics\n",
        PROG_NAME);
}

void config_init(struct config *config) {
    config->numeraria_fd = -1;
}

bool config_init_numeraria(struct config *config) {
    const char *const socket = config->input.numeraria_socket;
    const char *const unix = config->input.numeraria_unix;
    if(*socket) {
        if((config->numeraria_fd = mtrix_numeraria_init_socket(socket)) == -1)
            return false;
    } else if(*unix) {
        if((config->numeraria_fd = mtrix_numeraria_init_unix(unix)) == -1)
            return false;
    }
    return true;
}

bool config_destroy(struct config *config) {
    bool ret = true;
    if(config->numeraria_fd != -1 && close(config->numeraria_fd) == -1) {
        log_errno("%s: close numeraria_fd", __func__);
        ret = false;
    }
    return ret;
}

bool handle_cmd(struct config *config, const char *const *argv) {
    const char *const name = *argv;
    const mtrix_hash arg_hash = mtrix_hash_str(name);
    static_assert(!offsetof(struct mtrix_cmd, name_hash));
    struct mtrix_cmd *const cmd = bsearch(
        &arg_hash, COMMANDS,
        sizeof(COMMANDS) / sizeof(*COMMANDS) - 1, sizeof(*COMMANDS),
        mtrix_hash_cmp);
    if(!cmd) {
        log_err("unknown command: %s\n", name);
        usage(stderr);
        return false;
    }
    if((cmd->flags & NEEDS_RNG) && (!config->flags & RND_INITIALIZED))
        rnd_init(config);
    CMD_NAME = name;
    return cmd->f(config, argv + 1)
        && config_record_command(config, argv);
}

bool handle_file(struct config *config, FILE *f) {
    bool ret = true;
    char *buffer = NULL;
    size_t len = 0;
    for(;;) {
        if(getline(&buffer, &len, f) == -1)
            break;
        enum { CMD = 1, TERM = 1, ARGV_LEN = CMD + MTRIX_MAX_ARGS + TERM };
        char *argv[ARGV_LEN] = {0};
        if(!str_to_args(buffer, CMD + MTRIX_MAX_ARGS, argv)) {
            log_err("%s: too many arguments\n", argv[0]);
            ret = false;
            break;
        }
        const bool ret = handle_cmd(config, (const char * const*)argv);
        CMD_NAME = NULL;
        if(!ret)
            break;
    }
    if(buffer)
        free(buffer);
    return ret;
}

bool str_to_args(char *str, size_t max_args, char **argv) {
    const char *d = " \n";
    size_t n = 0;
    char *saveptr = NULL, *arg = strtok_r(str, d, &saveptr);
    while(n < max_args && arg) {
        argv[n++] = arg;
        arg = strtok_r(NULL, d, &saveptr);
    }
    return n < max_args || !arg;
}

const char *rnd_list_item(const char **v, size_t n) {
    return v[(size_t)rand() % n];
}

const char *list_item(const char **v, size_t n, const char *i_str) {
    int64_t i = parse_i64(i_str);
    if(i == -1)
        return NULL;
    --i;
    if(i < 0 || n <= (size_t)i) {
        fprintf(stderr, "no item with index %" PRIi64 "\n", i + 1);
        return NULL;
    }
    return v[i];
}

bool config_record_command(
    const struct config *config, const char *const *argv
) {
    if(config->numeraria_fd == -1)
        return true;
    size_t n_args = 0, len = sizeof(size_t), arg_len[MTRIX_MAX_ARGS + 1];
    const char *args[MTRIX_MAX_ARGS + 1] = {0};
    for(; *argv; ++n_args, ++argv) {
        const size_t i_len = strlen(*argv);
        args[n_args] = *argv;
        arg_len[n_args] = i_len;
        len += sizeof(size_t) + i_len;
        if(MTRIX_NUMERARIA_MAX_CMD < len)
            return log_err("%s: command too long\n", __func__), false;
    }
    if(MTRIX_NUMERARIA_MAX_CMD < len)
        return log_err("%s: command too long\n", __func__), false;
    union {
        struct mtrix_numeraria_cmd h;
        char buffer[MTRIX_NUMERARIA_MAX_PACKET];
    } header;
    char *p = header.h.data;
    memcpy(p, &n_args, sizeof(n_args));
    p += sizeof(n_args);
    for(size_t i = 0; i < n_args; ++i) {
        memcpy(p, &arg_len[i], sizeof(arg_len[i]));
        p += sizeof(arg_len[i]);
        memcpy(p, args[i], arg_len[i]);
        p += arg_len[i];
    }
    header.h.cmd = MTRIX_NUMERARIA_CMD_RECORD_CMD;
    header.h.len = (uint32_t)len;
    char ret;
    return write_all(
            config->numeraria_fd, &header,
            MTRIX_NUMERARIA_CMD_SIZE + header.h.len)
        && read_all(config->numeraria_fd, &ret, sizeof(ret))
        && ret == 0;
}

bool cmd_help(const struct config *config, const char *const *argv) {
    (void)config;
    if(argv[0])
        return log_err("command accepts no arguments\n"), false;
    usage(stdout);
    return true;
}

bool cmd_ping(const struct config *config, const char *const *argv) {
    (void)config;
    if(argv[0])
        return log_err("command accepts no arguments\n"), false;
    printf("pong\n");
    return true;
}

bool cmd_word(const struct config *config, const char *const *argv) {
    (void)config;
    if(argv[0])
        return log_err("command accepts no arguments\n"), false;
    pid_t pid = fork();
    if(pid == -1) {
        log_errno("fork");
        return false;
    }
    if(!pid)
        return exec(
            (const char *[]){"shuf", "-n", "1", DICT_FILE, 0},
            -1, -1, -1);
    return wait_n(1, &pid);
}

bool cmd_abbr(const struct config *config, const char *const *argv) {
    (void)config;
    const char *const abbr = *argv++;
    if(!abbr)
        return log_err("command takes at least one argument\n"), false;
    char dict_buf[MAX_PATH];
    const char *dict = *argv++;
    if(dict) {
        if(!(dict = join_path(dict_buf, 2, "/usr/share/dict/", dict)))
            return false;
        if(*argv)
            return log_err("command takes at most two arguments\n"), false;
    }
    char *buffer = NULL;
    size_t buffer_len = 0;
    for(const char *c = abbr; *c; ++c) {
        int fds[2][2];
        if(pipe(fds[0]) == -1) {
            log_errno("pipe");
            return false;
        }
        pid_t pids[2] = {0};
        pids[0] = fork();
        if(pids[0] == -1) {
            log_errno("fork");
            return false;
        }
        if(!pids[0]) {
            close(fds[0][0]);
            return exec(
                (const char*[]){
                    "look", "--ignore-case", (char[]){*c, '\0'}, dict, 0,
                },
                -1, fds[0][1], -1);
        }
        close(fds[0][1]);
        if(pipe(fds[1]) == -1) {
            log_errno("pipe");
            return false;
        }
        pids[1] = fork();
        if(!pids[1]) {
            close(fds[1][0]);
            return exec(
                (const char*[]){"shuf", "-n", "1", 0},
                fds[0][0], fds[1][1], -1);
        }
        close(fds[0][0]);
        close(fds[1][1]);
        FILE *child = fdopen(fds[1][0], "r");
        if(!child) {
            log_errno("fdopen");
            return false;
        }
        ssize_t len = 0;
        if((len = getline(&buffer, &buffer_len, child)) == -1)
            break;
        if(buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';
        printf("%s%s", c != abbr ? " " : "", buffer);
        if(!wait_n(2, pids))
            return false;
    }
    printf("\n");
    return true;
}

bool cmd_damn(const struct config *config, const char *const *argv) {
    (void)config;
    if(argv[0] && argv[1])
        return log_err("command accepts at most one argument\n"), false;
    int fds[2];
    if(pipe(fds) == -1) {
        log_errno("pipe");
        return false;
    }
    pid_t pid = fork();
    if(pid == -1) {
        log_errno("fork");
        return false;
    }
    if(!pid) {
        close(fds[0]);
        return exec(
            (const char *[]){"shuf", DICT_FILE, "-n", *argv ? *argv : "3", 0},
            -1, fds[1], -1);
    }
    close(fds[1]);
    FILE *child = fdopen(fds[0], "r");
    if(!child) {
        log_errno("fdopen");
        return false;
    }
    printf("You");
    char *buffer = 0;
    size_t buffer_len = 0;
    for(;;) {
        ssize_t len = 0;
        if((len = getline(&buffer, &buffer_len, child)) == -1)
            break;
        if(buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';
        printf(" %s", buffer);
    }
    printf("!\n");
    if(buffer)
        free(buffer);
    return wait_n(1, &pid);
}

bool cmd_parl(const struct config *config, const char *const *argv) {
    (void)config;
    if(argv[0])
        return log_err("command accepts no arguments\n"), false;
    printf("%s", RND_LIST_ITEM(((const char*[]){
        "A terminological inexactitude [citation needed].\n",
        "Liar.\n-- Australia, 1997\n",
        "Dumbo.\n-- Australia, 1997\n",
        "In Belgium there is no such thing as unparliamentary language.\n"
        "-- Belgium\n",
        "Parliamentary pugilist.\n-- Canada, 1875\n",
        "A bag of wind.\n-- Canada, 1878\n",
        "Inspired by forty-rod whisky.\n-- Canada, 1881\n",
        "Coming into the world by accident.\n-- Canada, 1886\n",
        "Blatherskite.\n-- Canada, 1890\n",
        "The political sewer pipe from Carleton County.\n-- Canada, 1917\n",
        "Lacking in intelligence.\n-- Canada, 1934\n",
        "A dim-witted saboteur.\n-- Canada, 1956\n",
        "Liar.\n-- Canada, 1959\n",
        "Devoid of honour.\n-- Canada, 1960\n",
        "Joker in the house.\n-- Canada, 1960\n",
        "Ignoramus.\n-- Canada, 1961\n",
        "Scurrilous.\n-- Canada, 1961\n",
        "To hell with Parliament attitude.\n-- Canada, 1961\n",
        "Trained seal.\n-- Canada, 1961\n",
        "Evil genius.\n-- Canada, 1962\n",
        "Demagogue.\n-- Canada, 1963\n",
        "Canadian Mussolini.\n-- Canada, 1964\n",
        "Sick animal.\n-- Canada, 1966\n",
        "Pompous ass.\n-- Canada, 1967\n",
        "Crook.\n-- Canada, 1971\n",
        "Does not have a spine.\n-- Canada, 1971\n",
        "Fuddle duddle.\n-- Canada, 1971\n",
        "Pig.\n-- Canada, 1977\n",
        "Jerk.\n-- Canada, 1980\n",
        "Sleazebag.\n-- Canada, 1984\n",
        "Racist.\n-- Canada, 1986\n",
        "Scuzzball.\n-- Canada, 1988\n",
        "Weathervane.\n-- Canada, 2007\n",
        "A piece of shit.\n-- Canada, 2011\n",
        "Like a fart.\n-- Canada, 2016\n",
        "臭罌出臭草 (foul grass grows out of a foul ditch).\n"
        "-- Hong Kong, 1996\n",
        "Bad man.\n-- India, 2012\n",
        "Badmashi.\n-- India, 2012\n",
        "Bag of shit.\n-- India, 2012\n",
        "Bandicoot.\n-- India, 2012\n",
        "Communist.\n-- India, 2012\n",
        "Double-minded.\n-- India, 2012\n",
        "Goonda.\n-- India, 2012\n",
        "Rat.\n-- India, 2012\n",
        "Ringmaster.\n-- India, 2012\n",
        "Scumbag.\n-- India, 2012\n",
        "Benny.\n-- Ireland\n",
        "Pair of bennies.\n-- Ireland\n",
        "Brat.\n-- Ireland\n",
        "Buffoon.\n-- Ireland\n",
        "Chancer.\n-- Ireland\n",
        "Communist.\n-- Ireland\n",
        "Corner boy.\n-- Ireland\n",
        "Coward.\n-- Ireland\n",
        "Fascist.\n-- Ireland\n",
        "Gurrier.\n-- Ireland\n",
        "Guttersnipe.\n-- Ireland\n",
        "Hypocrite.\n-- Ireland\n",
        "Rat.\n-- Ireland\n",
        "Scumbag.\n-- Ireland\n",
        "Scurrilous speaker.\n-- Ireland\n",
        "Yahoo.\n-- Ireland\n",
        "Lying or drunk; has violated the secrets of cabinet, "
        "or doctored an official report.\n-- Ireland\n",
        "Handbagging.\n-- Ireland\n",
        "Fuck you!\n-- Ireland, 2009\n",
        "Gurriers shouting on a street at each other.\n-- Ireland\n",
        "Idle vapourings of a mind diseased.\n-- New Zealand, 1946\n",
        "His brains could revolve inside a peanut shell for a thousand years "
        "without touching the sides.\n-- New Zealand, 1949\n",
        "Energy of a tired snail returning home from a funeral.\n"
        "-- New Zealand, 1963\n",
        "Commo (meaning communist).\n-- New Zealand, 1969\n",
        "Scuttles for his political funk hole.\n-- New Zealand, 1974\n",
        "Highway bandit.\n-- Norway, 2009\n",
        "Bastard.\n-- United Kingdom\n",
        "Blackguard.\n-- United Kingdom\n",
        "Coward.\n-- United Kingdom\n",
        "Deceptive.\n-- United Kingdom\n",
        "Dodgy.\n-- United Kingdom\n",
        "Drunk.\n-- United Kingdom\n",
        "Falsehoods.\n-- United Kingdom\n",
        "Git.\n-- United Kingdom\n",
        "Guttersnipe.\n-- United Kingdom\n",
        "Hooligan.\n-- United Kingdom\n",
        "Hypocrite.\n-- United Kingdom\n",
        "Idiot.\n-- United Kingdom\n",
        "Ignoramus.\n-- United Kingdom\n",
        "Liar.\n-- United Kingdom\n",
        "Pipsqueak.\n-- United Kingdom\n",
        "Rat.\n-- United Kingdom\n",
        "Slimy.\n-- United Kingdom\n",
        "Sod.\n-- United Kingdom\n",
        "Squirt.\n-- United Kingdom\n",
        "Stoolpigeon.\n-- United Kingdom\n",
        "Swine.\n-- United Kingdom\n",
        "Tart.\n-- United Kingdom\n",
        "Traitor.\n-- United Kingdom\n",
        "Wart.\n-- United Kingdom\n",
        "Crooked deals.\n-- United Kingdom\n",
        "Use of banned substances.\n-- United Kingdom\n",
        "Been bought.\n-- United Kingdom\n",
        "Racist.\n-- United Kingdom\n",
        "Has made a career out of lying.\n-- United Kingdom\n",
        "Lying.\n-- Wales\n",
        "Mrs. Windsor.\n-- Wales\n",
        "Economical with the truth.\n",
        "Tired and emotional.\n-- United Kingdom\n",
        "Incapable.\n-- United Kingdom\n",
    })));
    return true;
}

bool cmd_bard(const struct config *config, const char *const *argv) {
    (void)config;
    if(argv[0])
        return log_err("command accepts no argument\n"), false;
    printf("%s", RND_LIST_ITEM(((const char*[]){
        "A most notable coward, an infinite and endless liar, an hourly"
        " promise breaker, the owner of no one good quality.\n"
        "-- All’s Well That Ends Well (Act 3, Scene 6)\n",
        "Away, you starvelling, you elf-skin, you dried neat’s-tongue,"
        " bull’s-pizzle, you stock-fish!\n"
        "-- Henry IV Part I (Act 2, Scene 4)\n",
        "Away, you three-inch fool!\n"
        "-- The Taming of the Shrew (Act 3, Scene 3)\n",
        "Come, come, you froward and unable worms!\n"
        "-- The Taming Of The Shrew (Act 5, Scene 2)\n",
        "Go, prick thy face, and over-red thy fear, Thou lily-liver’d boy.\n"
        "-- Macbeth (Act 5, Scene 3)\n",
        "His wit’s as thick as a Tewkesbury mustard.\n"
        "-- Henry IV Part 2 (Act 2, Scene 4)\n",
        "I am pigeon-liver’d and lack gall.\n"
        "-- Hamlet (Act 2, Scene 2)\n",
        "I am sick when I do look on thee\n"
        "-- A Midsummer Night’s Dream (Act 2, Scene 1)\n",
        "I must tell you friendly in your ear, sell when you can, you are not"
        " for all markets.\n"
        "-- As You Like It (Act 3 Scene 5)\n",
        "If thou wilt needs marry, marry a fool; for wise men know well enough"
        " what monsters you make of them.\n"
        "-- Hamlet (Act 3, Scene 1)\n",
        "I’ll beat thee, but I would infect my hands.\n"
        "-- Timon of Athens (Act 4, Scene 3)\n",
        "I scorn you, scurvy companion.\n"
        "-- Henry IV Part II (Act 2, Scene 4)\n",
        "Methink’st thou art a general offence and every man should beat thee."
        "\n"
        "-- All’s Well That Ends Well (Act 2, Scene 3)\n",
        "More of your conversation would infect my brain.\n"
        "-- The Comedy of Erros (Act 2, Scene 1)\n",
        "My wife’s a hobby horse!\n"
        "-- The Winter’s Tale (Act 2, Scene 1)\n",
        "Peace, ye fat guts!\n"
        "-- Henry IV Part 1 (Act 2, Scene 2)\n",
        "Poisonous bunch-backed toad! \n"
        "-- Richard III (Act 1, Scene 3)\n",
        "The rankest compound of villainous smell that ever offended nostril\n"
        "-- The Merry Wives of Windsor (Act 3, Scene 5)\n",
        "The tartness of his face sours ripe grapes.\n"
        "-- The Comedy of Erros (Act 5, Scene 4)\n",
        "There’s no more faith in thee than in a stewed prune.\n"
        "-- Henry IV Part 1 (Act 3, Scene 3)\n",
        "Thine forward voice, now, is to speak well of thine friend; thine"
        " backward voice is to utter foul speeches and to detract.\n"
        "-- The Tempest (Act 2, Scene 2)\n",
        "That trunk of humours, that bolting-hutch of beastliness, that"
        " swollen parcel of dropsies, that huge bombard of sack, that"
        " stuffed cloak-bag of guts, that roasted Manningtree ox with"
        " pudding in his belly, that reverend vice, that grey Iniquity,"
        " that father ruffian, that vanity in years?\n"
        "-- Henry IV Part 1 (Act 2, Scene 4)\n",
        "Thine face is not worth sunburning.\n"
        "-- Henry V (Act 5, Scene 2)\n",
        "This woman’s an easy glove, my lord, she goes off and on at"
        " pleasure.\n"
        "-- All’s Well That Ends Well (Act 5, Scene 3)\n",
        "Thou art a boil, a plague sore\n"
        "-- King Lear (Act 2, Scene 2)\n",
        "Was the Duke a flesh-monger, a fool and a coward?\n"
        "-- Measure For Measure (Act 5, Scene 1)\n",
        "Thou art as fat as butter.\n"
        "-- Henry IV Part 1 (Act 2, Scene 4)\n",
        "Here is the babe, as loathsome as a toad.\n"
        "-- Titus Andronicus (Act 4, Scene 3)\n",
        "Like the toad; ugly and venomous.\n"
        "-- As You Like It (Act 2, Scene 1`)\n",
        "Thou art unfit for any place but hell.\n"
        "-- Richard III (Act 1 Scene 2)\n",
        "Thou cream faced loon\n"
        "-- Macbeth (Act 5, Scene 3)\n",
        "Thou clay-brained guts, thou knotty-pated fool, thou whoreson obscene"
        " greasy tallow-catch!\n"
        "-- Henry IV Part 1 (Act 2, Scene 4 )\n",
        "Thou damned and luxurious mountain goat.\n"
        "-- Henry V (Act 4, Scene 4)\n",
        "Thou elvish-mark’d, abortive, rooting hog!\n"
        "-- Richard III (Act 1, Scene 3 )\n",
        "Thou leathern-jerkin, crystal-button, knot-pated, agatering,"
        " puke-stocking, caddis-garter, smooth-tongue, Spanish pouch!\n"
        "-- Henry IV Part 1 (Act 2, Scene 4)\n",
        "Thou lump of foul deformity\n"
        "-- Richard III (Act 1, Scene 2)\n",
        "That poisonous bunch-back’d toad!\n"
        "-- Richard III (Act 1, Scene 3)\n",
        "Thou sodden-witted lord! Thou hast no more brain than I have in mine"
        " elbows\n"
        "-- Troilus and Cressida (Act 2, Scene 1)\n",
        "Thou subtle, perjur’d, false, disloyal man!\n"
        "-- The Two Gentlemen of Verona (Act 4, Scene 2)\n",
        "Thou whoreson zed , thou unnecessary letter!\n"
        "-- King Lear (Act 2, Scene 2)\n",
        "Thy sin’s not accidental, but a trade.\n"
        "-- Measure For Measure (Act 3, Scene 1)\n",
        "Thy tongue outvenoms all the worms of Nile.\n"
        "-- Cymbeline (Act 3, Scene 4)\n",
        "Would thou wert clean enough to spit upon\n"
        "-- Timon of Athens (Act 4, Scene 3)\n",
        "Would thou wouldst burst!\n"
        "-- Timon of Athens (Act 4, Scene 3)\n",
        "You poor, base, rascally, cheating lack-linen mate! \n"
        "-- Henry IV Part II (Act 2, Scene 4)\n",
        "You are as a candle, the better burnt out.\n"
        "-- Henry IV Part 2 (Act 1, Scene 2)\n",
        "You scullion! You rampallian! You fustilarian! I’ll tickle your"
        " catastrophe!\n"
        "-- Henry IV Part 2 (Act 2, Scene 1)\n",
        "You starvelling, you eel-skin, you dried neat’s-tongue, you"
        " bull’s-pizzle, you stock-fish–O for breath to utter what is like"
        " thee!-you tailor’s-yard, you sheath, you bow-case, you vile"
        " standing tuck!\n"
        "-- Henry IV Part 1 (Act 2, Scene 4)\n",
        "Your brain is as dry as the remainder biscuit after voyage.\n"
        "-- – As You Like It (Act 2, Scene 7)\n",
        "Virginity breeds mites, much like a cheese.\n"
        "-- All’s Well That Ends Well (Act 1, Scene 1)\n",
        "Villain, I have done thy mother\n"
        "-- Titus Andronicus (Act 4, Scene 2)\n",
    })));
    return true;
}

bool cmd_dlpo(const struct config *config, const char *const *argv) {
    if(!argv[0] || argv[1])
        return log_err("command takes one argument\n"), false;
    char url[MTRIX_MAX_URL_LEN];
    if(!BUILD_URL(url, DLPO_BASE "/", *argv))
        return false;
    if(mtrix_config_verbose(&config->c))
        printf("Looking up term: %s\n", url);
    if(mtrix_config_dry(&config->c))
        return true;
    TidyDoc tidy_doc = request_and_parse(url, mtrix_config_verbose(&config->c));
    if(!tidy_doc)
        return false;
    const char *id = "resultados";
    TidyNode res = find_node_by_id(tidyGetRoot(tidy_doc), id, true);
    bool ret = false;
    if(!res) {
        log_err("element '#%s' not found\n", id);
        goto cleanup;
    }
    TidyNode def = dlpo_find_definitions(res);
    if(!def)
        goto cleanup;
    TidyBuffer buf = {0};
    dlpo_print_definitions(stdout, tidy_doc, def, &buf);
    if(buf.bp)
        tidyBufFree(&buf);
    ret = true;
    stats_increment(config, STATS_DLPO);
cleanup:
    tidyRelease(tidy_doc);
    return ret;
}

bool cmd_wikt(const struct config *config, const char *const *argv) {
    const char *const term = *argv++;
    if(!term)
        return log_err("command takes at least one argument\n"), false;
    const char *const lang = *argv++;
    if(lang && *argv)
        return log_err("command takes at most two arguments\n"), false;
    char url[MTRIX_MAX_URL_LEN];
    if(!BUILD_URL(url, WIKTIONARY_BASE "/", term))
        return false;
    if(mtrix_config_verbose(&config->c))
        printf("Looking up term: %s\n", url);
    if(mtrix_config_dry(&config->c))
        return true;
    TidyDoc tidy_doc = request_and_parse(url, mtrix_config_verbose(&config->c));
    if(!tidy_doc)
        return false;
    bool ret = false;
    wikt_page page;
    if(!wikt_parse_page(tidy_doc, &page))
        goto cleanup;
    TidyBuffer buf = {0};
    TidyNode lang_sect = find_node_by_class(page.contents, WIKTIONARY_H2, true);
    while(lang_sect) {
        TidyNode sect = lang_sect;
        TidyAttr lang_id = find_attr(tidyGetChild(lang_sect), "id");
        const char *lang_text = lang_id ? tidyAttrValue(lang_id) : "?";
        if(lang && strcasecmp(lang, lang_text) != 0) {
            wikt_next_section(WIKTIONARY_H2, "", &lang_sect);
            continue;
        }
        while(wikt_next_section(WIKTIONARY_H3, "Etymology", &sect)) {
            if(lang_text) {
                printf("%s\n", lang_text);
                lang_text = NULL;
            }
            for(TidyNode etym = sect; (etym = tidyGetNext(etym));) {
                if(node_has_class(etym, WIKTIONARY_HEADER))
                    break;
                tidyNodeGetText(tidy_doc, etym, &buf);
                join_lines(buf.bp, buf.bp + buf.size);
                printf("  ");
                print_unescaped(stdout, buf.bp);
                printf("\n");
                tidyBufClear(&buf);
            }
        }
        if((lang_sect = sect))
            wikt_next_section(WIKTIONARY_H2, "", &lang_sect);
    }
    ret = true;
    if(buf.bp)
        tidyBufFree(&buf);
    stats_increment(config, STATS_WIKT);
cleanup:
    tidyRelease(tidy_doc);
    return ret;
}

bool cmd_tr(const struct config *config, const char *const *argv) {
    const char *const term = *argv++;
    if(!term)
        return log_err("command takes at least one argument\n"), false;
    const char *const lang = *argv++;
    if(lang && *argv)
        return log_err("command takes at most one argument\n"), false;
    char url[MTRIX_MAX_URL_LEN];
    if(!BUILD_URL(url, WIKTIONARY_BASE "/", term))
        return false;
    if(mtrix_config_verbose(&config->c))
        printf("Looking up term: %s\n", url);
    if(mtrix_config_dry(&config->c))
        return true;
    TidyDoc tidy_doc = request_and_parse(url, mtrix_config_verbose(&config->c));
    if(!tidy_doc)
        return false;
    bool ret = false;
    wikt_page page;
    if(!wikt_parse_page(tidy_doc, &page))
        goto cleanup;
    TidyBuffer buf = {0};
    for(TidyNode lang_sect = page.contents; lang_sect;) {
        TidyNode sect = lang_sect;
        TidyAttr lang_id = find_attr(tidyGetChild(lang_sect), "id");
        const char *lang_text = lang_id ? tidyAttrValue(lang_id) : "?";
        while(wikt_next_subsection(NULL, "Translations-", &sect)) {
            if(lang_text) {
                printf("%s\n", lang_text);
                lang_text = NULL;
            }
            const TidyNode head = wikt_translation_head(sect);
            if(!head)
                continue;
            tidyNodeGetText(tidy_doc, head, &buf);
            join_lines(buf.bp, buf.bp + buf.size);
            printf("  %s\n", buf.bp);
            tidyBufClear(&buf);
            const TidyNode body = wikt_translation_body(sect);
            if(!body)
                continue;
            TidyNode td = body, li;
            while((td = wikt_next_translation_block(td, &li))) {
                for(; li; li = tidyGetNext(li)) {
                    tidyNodeGetText(tidy_doc, li, &buf);
                    join_lines(buf.bp, buf.bp + buf.size);
                    if(!lang || wikt_translation_is_language(buf, lang)) {
                        printf("    ");
                        print_unescaped(stdout, buf.bp);
                        printf("\n");
                    }
                    tidyBufClear(&buf);
                }
                td = tidyGetNext(td);
            }
        }
        lang_sect = sect;
    }
    ret = true;
    if(buf.bp)
        tidyBufFree(&buf);
cleanup:
    tidyRelease(tidy_doc);
    return ret;
}

bool cmd_aoe(const char **v, size_t n, const char *const *argv) {
    const char *const i = *argv;
    if(i && argv[1])
        return log_err("command accepts at most one argument\n"), false;
    if(!i) {
        printf("%s", rnd_list_item(v, n));
        return true;
    }
    const char *const s = list_item(v, n, i);
    if(!s)
        return false;
    printf("%s", s);
    return true;
}

bool cmd_aoe1(const struct config *config, const char *const *argv) {
    (void)config;
    static const char *v[] = {
        "Yes.\n",
        "No.\n",
        "I need food.\n",
        "Somebody pass the wood.\n",
        "Gold please.\n",
        "Gimme some stone.\n",
        "*Whimper*\n",
        "Your attempts are futile.\n",
        "*Group cheer*\n",
        "Hey, I'm in your town.\n",
        "*Group groan*\n",
        "Join me!\n",
        "I don't think so.\n",
        "Start the game already!\n",
        "Who's the man?\n",
        "Attack them now!\n",
        "*Low laugh*\n",
        "I am weak, please don't kill me!\n",
        "*High pitched laugh*\n",
        "I just got some... satisfaction!\n",
        "Hey, nice town!\n",
        "We will NOT tolerate this behavior.\n",
        "Get out!\n",
        "Dad gum!\n",
        "Aw, yeah!\n",
    };
    return cmd_aoe(v, ARRAY_SIZE(v), argv);
}

bool cmd_aoe2(const struct config *config, const char *const *argv) {
    (void)config;
    static const char *v[] = {
        "Yes.\n",
        "No.\n",
        "Food please.\n",
        "Wood please.\n",
        "Gold please.\n",
        "Stone please.\n",
        "Ahh!\n",
        "All hail, king of the losers!\n",
        "Ooh!\n",
        "I'll beat you back to Age of Empires.\n",
        "(Herb laugh)\n",
        "Ah! being rushed.\n",
        "Sure, blame it on your ISP.\n",
        "Start the game already!\n",
        "Don't point that thing at me!\n",
        "Enemy sighted!\n",
        "It is good to be the king.\n",
        "Monk! I need a monk!\n",
        "Long time, no siege.\n",
        "My granny could scrap better than that.\n",
        "Nice town, I'll take it.\n",
        "Quit touching me!\n",
        "Raiding party!\n",
        "Dadgum.\n",
        "Eh, smite me.\n",
        "The wonder, the wonder, the... no!\n",
        "You played two hours to die like this?\n",
        "Yeah, well, you should see the other guy.\n",
        "Roggan.\n",
        "Wololo.\n",
        "Attack an enemy now.\n",
        "Cease creating extra villagers.\n",
        "Create extra villagers.\n",
        "Build a navy.\n",
        "Stop building a navy.\n",
        "Wait for my signal to attack.\n",
        "Build a wonder.\n",
        "Give me your extra resources.\n",
        "(Ally sound)\n",
        "(Neutral sound)\n",
        "(Enemy sound)\n",
        "What age are you in?\n",
        "What is your strategy?\n",
        "How many resources do you have?\n",
        "Retreat now!\n",
        "Flare the location of your army.\n",
        "Attack in direction of the flared location.\n",
        "I'm being attacked, please help!\n",
        "Build a forward base at the flared location.\n",
        "Build a fortification at the flared location.\n",
        "Keep your army close to mine and fight with me.\n",
        "Build a market at the flared location.\n",
        "Rebuild your base at the flared location.\n",
        "Build a wall between the two flared locations.\n",
        "Build a wall around your town.\n",
        "Train units which counter the enemy's army.\n",
        "Stop training counter units.\n",
        "Prepare to send me all your resources so I can vanquish our foes!\n",
        "Stop sending me extra resources.\n",
        "Prepare to train a large army, I will send you as many resources as I"
            " can spare.\n",
        "Attack player 1! (Blue)\n",
        "Attack player 2! (Red)\n",
        "Attack player 3! (Green)\n",
        "Attack player 4! (Yellow)\n",
        "Attack player 5! (Cyan)\n",
        "Attack player 6! (Purple)\n",
        "Attack player 7! (Gray)\n",
        "Attack player 8! (Orange)\n",
        "Delete the object on the flared location.\n",
        "Delete your excess villagers.\n",
        "Delete excess warships.\n",
        "Focus on training infantry units.\n",
        "Focus on training cavalry units.\n",
        "Focus on training ranged units.\n",
        "Focus on training warships.\n",
        "Attack the enemy with Militia.\n",
        "Attack the enemy with Archers.\n",
        "Attack the enemy with Skirmishers.\n",
        "Attack the enemy with a mix of Archers and Skirmishers.\n",
        "Attack the enemy with Scout Cavalry.\n",
        "Attack the enemy with Men-at-Arms.\n",
        "Attack the enemy with Eagle Scouts.\n",
        "Attack the enemy with Towers.\n",
        "Attack the enemy with Crossbowmen.\n",
        "Attack the enemy with Cavalry Archers.\n",
        "Attack the enemy with Unique Units.\n",
        "Attack the enemy with Knights.\n",
        "Attack the enemy with Battle Elephants.\n",
        "Attack the enemy with Scorpions.\n",
        "Attack the enemy with Monks.\n",
        "Attack the enemy with Monks and Mangonels.\n",
        "Attack the enemy with Eagle Warriors.\n",
        "Attack the enemy with Halberdiers and Rams.\n",
        "Attack the enemy with Elite Eagle Warriors.\n",
        "Attack the enemy with Arbalests.\n",
        "Attack the enemy with Champions.\n",
        "Attack the enemy with Galleys.\n",
        "Attack the enemy with Fire Galleys.\n",
        "Attack the enemy with Demolition Rafts.\n",
        "Attack the enemy with War Galleys.\n",
        "Attack the enemy with Fire Ships.\n",
        "Attack the enemy with Unique Warships.\n",
        "Use an Onager to cut down trees at the flared location.\n",
        "Don't resign!\n",
        "You can resign again.\n",
    };
    return cmd_aoe(v, ARRAY_SIZE(v), argv);
}

bool stats_increment(const struct config *config, uint8_t opt) {
    const char *const path = config->input.stats_file;
    if(!*path)
        return true;
    FILE *const f = open_or_create(path, "r+");
    if(!f)
        return false;
    struct stats s = {0};
    fread(&s, sizeof(s), 1, f);
    bool ret = false;
    if(ferror(f)) {
        log_errno("%s: fread", __func__);
        goto end;
    }
    switch(opt) {
    case STATS_WIKT: s.wikt++; break;
    case STATS_DLPO: s.dlpo++; break;
    default: assert(!"invalid option");
    }
    rewind(f);
    if(fwrite(&s, sizeof(s), 1, f) != 1) {
        log_errno("%s: fwrite", __func__);
        goto end;
    }
    ret = true;
end:
    if(fclose(f) == -1) {
        log_errno("%s: fclose", __func__);
        ret = false;
    }
    return ret;
}

bool cmd_stats(const struct config *config, const char *const *argv) {
    if(argv[0])
        return log_err("command takes one argument\n"), false;
    return stats_file(config) && stats_numeraria(config);
}

bool stats_file(const struct config *config) {
    const char *path = config->input.stats_file;
    if(!*path)
        return true;
    struct stats s = {0};
    FILE *const f = fopen(path, "r");
    if(!f) {
        if(errno != ENOENT)
            return log_errno("%s: fopen(%s)", __func__, path), false;
    } else {
        if(fread(&s, sizeof(s), 1, f) != 1)
            return log_errno("%s: fread", __func__), false;
        if(fclose(f) == -1)
            return log_errno("%s: close", __func__), false;
    }
    printf("file\n  wikt: %d\n  dlpo: %d\n", s.wikt, s.dlpo);
    return true;
}

bool stats_numeraria(const struct config *config) {
    if(config->numeraria_fd == -1)
        return true;
    printf("numeraria\n");
    const struct mtrix_numeraria_cmd header =
        {.cmd = MTRIX_NUMERARIA_CMD_STATS};
    if(!write_all(config->numeraria_fd, &header, MTRIX_NUMERARIA_CMD_SIZE))
        return log_err("failed to send numeraria command\n"), false;
    for(;;) {
        size_t n_cols = 0;
        if(!read_all(config->numeraria_fd, &n_cols, sizeof(n_cols)))
            return log_err("failed to read numeraria columns\n"), false;
        if(!n_cols)
            break;
        for(size_t i = 0; i < n_cols; ++i) {
            size_t len = 0;
            if(!read_all(config->numeraria_fd, &len, sizeof(len))) {
                log_err("failed to read numeraria result length\n");
                return false;
            }
            char buffer[MTRIX_NUMERARIA_MAX_CMD];
            if(len && !read_all(config->numeraria_fd, buffer, len))
                return log_err("failed to read numeraria result\n"), false;
            if(i)
                printf(" \"%.*s\"", (int)len, buffer);
            else
                printf("  %.*s", (int)len, buffer);
        }
        printf("\n");
    }
    return true;
}
