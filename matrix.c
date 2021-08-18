/**
 * \file
 * Program that interacts with the Matrix server.  Executes the main program to
 * handle the commands.
 */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>

#include <cjson/cJSON.h>

#include "config.h"
#include "log.h"
#include "utils.h"

/** Root URL for the Matrix API. */
#define API_URL "/_matrix/client/r0"

/** URL for the synchronization endpoint. */
#define SYNC_URL API_URL "/sync"

/** Limits the number of synchronization events to one. */
#define ROOM_FILTER "filter={\"room\":{\"timeline\":{\"limit\":1}}}"

/** URL for the rooms endpoint. */
#define ROOMS_URL API_URL "/rooms"

/** URL fragment for the send-message enpoint for a room. */
#define SEND_URL "/send/m.room.message"

/** Polling interval for the synchronization request. */
#define SYNC_INTERVAL_MS_STR "30000"

/** Sets the polling interval to \ref SYNC_INTERVAL_MS_STR. */
#define TIMEOUT_PARAM "timeout=" SYNC_INTERVAL_MS_STR

/**
 * Prepends the server and appends the token from the configuration.
 * \param c Pointer to a \ref config object.
 */
#define URL_PARTS(c, ...) \
    (c)->server, __VA_ARGS__, "access_token=", (c)->token

/** Builds a URL using \ref URL_PARTS. */
#define BUILD_MATRIX_URL(c, url, ...) \
    BUILD_URL(url, URL_PARTS((c), __VA_ARGS__))

enum {
    /** Filter mode was chosen. */
    FILTER_FLAG = 1 << 0,
};

struct config {
    struct mtrix_config c;
    /** Extra `machinatrix` arguments, in `execvp` format. */
    const char **args;
    /** Filter, etc. */
    uint8_t flags;
};

/** To be filled by `argv[0]` later, for logging. */
const char *PROG_NAME = NULL;

/** Always null. */
const char *CMD_NAME = NULL;

/** Program entry point. */
int main(int argc, const char *const *argv);

/** Parses command-line arguments and fills `config`. */
static bool parse_args(
    int *argc, const char *const **argv, struct config *config);

/** Prints a usage message. */
static void usage(FILE *f);

/** Extracts the short user name from a `@user:server` string. */
static bool short_username(const char *user, char *out);

/**
 * Reads the token from a file.
 * \param b
       Buffer with the name of the file and maximum token length (including the
       null terminator).  Replaced with the contents of the token.
 */
static bool read_token(struct mtrix_buffer b);

/** Destructs `config`. */
static void config_destroy(struct config *config);

/** Constructs the `machinatrix` command line with references to `argv`. */
static void config_set_args(struct config *config, const char *const *argv);

/** Logs a message if verbose output was requested. */
static void config_verbose(const struct config *config, const char *fmt, ...);

/** Shortcut for `cJSON_GetObjectItemCaseSensitive. */
static cJSON *get_item(const cJSON *j, const char *k);

/** Fetches the inital batch for synchronization. */
static bool init_batch(const struct config *config, char *batch);

/** Extracts the next batch from a server response. */
static bool get_next_batch(cJSON *j, char *batch);

/** Main request/response loop in filter mode. */
static bool filter(const struct config *config, char *batch);

/** Main request/response loop. */
static bool loop(const struct config *config, char *batch);

/** Parses a string as JSON. */
static cJSON *parse_json(const char *s);

/** Handles a single request. */
static bool handle_request(
    const struct config *config, cJSON *root, size_t user_len,
    bool (*send_msg)(const struct config*, const char*, const char*));

/** Checks that an event has the expected type. */
static bool check_event_type(const cJSON *event, const char *value);

/** Finds the event body, if it contains one. */
static const char *event_body(const cJSON *event);

/** Finds the event sender, if it contains one. */
static const char *event_sender(const cJSON *event);

/** Checks if the a user was mentioned. */
static bool check_mention(const char *text, const char *user);

/** Invokes the main program. */
static bool process_input(
    const struct config *config,
    const char *input, struct mtrix_buffer *output);

/** Prints a message to stdout. */
static bool print_msg(
    const struct config *config, const char *room, const char *msg);

/** Sends a message to the room. */
static bool send_msg(
    const struct config *config, const char *room, const char *msg);

/** Consumes all output from `f`, appending it to `buf`. */
static bool read_output(FILE *f, struct mtrix_buffer *buf);

int main(int argc, const char *const *argv) {
    log_set(stderr);
    PROG_NAME = argv[0];
    struct config config = {0};
    if(!parse_args(&argc, &argv, &config))
        return 1;
    bool ret = false;
    if(config.c.flags & MTRIX_CONFIG_HELP) {
        usage(stdout);
        ret = true;
        goto end;
    }
    if(!*config.c.server) {
        log_err("no server specified\n");
        goto end;
    }
    if(!*config.c.user) {
        log_err("no user specified\n");
        goto end;
    }
    if(!*config.c.token) {
        log_err("no token specified\n");
        goto end;
    }
    if(mtrix_config_verbose(&config.c)) {
        log_err("using server: %s\n", config.c.server);
        log_err("using user: %s\n", config.c.user);
    }
    char batch[MAX_BATCH];
    if(*config.c.batch) {
        assert(strlen(config.c.batch) < sizeof(batch));
        strcpy(batch, config.c.batch);
    } else if(!init_batch(&config, batch))
        goto end;
    config_verbose(&config, "using batch: %s\n", batch);
    if(config.flags & FILTER_FLAG) {
        if(!filter(&config, batch))
            goto end;
    } else if(!loop(&config, batch))
        goto end;
    ret = true;
end:
    config_destroy(&config);
    return !ret;
}

bool parse_args(int *argc, const char *const **argv, struct config *config) {
    enum { HELP, VERBOSE, DRY, SERVER, USER, TOKEN, BATCH, FILTER };
    static const char *short_opts = "hvn";
    static const struct option long_opts[] = {
        [HELP] = {"help", no_argument, 0, 'h'},
        [VERBOSE] = {"verbose", no_argument, 0, 'v'},
        [DRY] = {"dry-run", no_argument, 0, 'n'},
        [SERVER] = {"server", required_argument, 0, 0},
        [USER] = {"user", required_argument, 0, 0},
        [TOKEN] = {"token", required_argument, 0, 0},
        [BATCH] = {"batch", required_argument, 0, 0},
        [FILTER] = {"filter", no_argument, 0, 0},
        {0},
    };
    for(;;) {
        int idx = 0;
        const int c = getopt_long(
            *argc, (char *const*)*argv, short_opts, long_opts, &idx);
        if(c == -1)
            break;
        switch(c) {
        case 'h': config->c.flags |= MTRIX_CONFIG_HELP; continue;
        case 'v': config->c.flags |= MTRIX_CONFIG_VERBOSE; continue;
        case 'n': config->c.flags |= MTRIX_CONFIG_DRY; continue;
        case 0: break;
        default: return false;
        }
        switch(idx) {
        case SERVER: {
            struct mtrix_buffer b = {.p = config->c.server, .n = MAX_SERVER};
            if(!copy_arg("server name", b, optarg))
                return false;
            break;
        }
        case USER: {
            struct mtrix_buffer b = {.p = config->c.user, .n = MAX_USER};
            if(!copy_arg("user name", b, optarg))
                return false;
            if(!short_username(config->c.user, config->c.short_user))
                return false;
            break;
        }
        case TOKEN: {
            struct mtrix_buffer b = {.p = config->c.token, .n = MAX_TOKEN};
            if(!(copy_arg("token", b, optarg) && read_token(b)))
                return false;
            break;
        }
        case BATCH: {
            struct mtrix_buffer b = {.p = config->c.batch, .n = MAX_BATCH};
            if(!copy_arg("batch", b, optarg))
                return false;
            break;
        }
        case FILTER: config->flags |= FILTER_FLAG; break;
        default: return false;
        }
    }
    const char *const *new_argv = *argv + optind;
    config_set_args(config, new_argv);
    *argv = new_argv;
    *argc -= optind;
    return true;
}

void usage(FILE *f) {
    fprintf(
        f,
        "usage: %s [options]\n\n"
        "Options:\n"
        "    -h, --help             this help\n"
        "    -v, --verbose          verbose output\n"
        "    -n, --dry-run          don't access external services\n"
        "        --server <arg>     matrix server (required)\n"
        "        --user   <arg>     matrix user (required)\n"
        "        --token  <arg>     matrix token file (required)\n"
        "        --batch  <arg>     matrix batch file\n"
        "        --filter           read message lines from stdin, write\n"
        "                           responses to stdout\n"
        "\n"
        "Additional positional arguments are forwarded to `machinatrix`.\n",
        PROG_NAME);
}

inline cJSON *get_item(const cJSON *j, const char *k) {
    return cJSON_GetObjectItemCaseSensitive(j, k);
}

bool short_username(const char *user, char *out) {
    if(*user != '@') {
        log_err("user missing \"@\" prefix\n");
        return false;
    }
    ++user;
    const char *colon = strchr(user, ':');
    if(!colon) {
        log_err("user missing \":\" character\n");
        return false;
    }
    assert(user <= colon);
    memcpy(out, user, (size_t)(colon - user));
    return true;
}

bool read_token(struct mtrix_buffer b) {
    bool ret = false;
    FILE *f = fopen(b.p, "rb");
    if(!f) {
        log_errno("fopen: %s", b.p);
        return false;
    }
    const char *const p = b.p;
    for(;;) {
        size_t n = fread(b.p, 1, b.n, f);
        if(ferror(f)) {
            log_errno("fread");
            break;
        }
        b.p += n;
        b.n -= n;
        if(!b.n) {
            log_err("token too long\n");
            break;
        }
        if(feof(f)) {
            ret = true;
            *b.p-- = 0;
            while(p <= b.p && *b.p == '\n')
                *b.p-- = 0;
            break;
        }
    }
    if(fclose(f) == EOF) {
        log_errno("fclose");
        return false;
    }
    return ret;
}

void config_destroy(struct config *config) {
    free(config->args);
}

void config_set_args(struct config *config, const char *const *argv) {
    size_t n = 0;
    for(const char *const *p = argv; *p; ++p)
        ++n;
    const char **ret = calloc(n + 2, sizeof(const char**));
    ret[0] = "machinatrix";
    for(size_t i = 0; i < n; ++i)
        ret[i + 1] = argv[i];
    ret[n + 1] = 0;
    config->args = ret;
}

void config_verbose(const struct config *config, const char *fmt, ...) {
    if(!mtrix_config_verbose(&config->c))
        return;
    va_list argp;
    va_start(argp, fmt);
    log_errv(fmt, argp);
    va_end(argp);
}

bool init_batch(const struct config *config, char *batch) {
    char url[MTRIX_MAX_URL_LEN];
    // TODO use Authorization header
    if(!BUILD_MATRIX_URL(&config->c, url, SYNC_URL "?" ROOM_FILTER "&"))
        return false;
    struct mtrix_buffer buffer = {NULL, 0};
    if(!request(url, &buffer, mtrix_config_verbose(&config->c))) {
        free(buffer.p);
        return false;
    }
    cJSON *root = cJSON_Parse(buffer.p);
    free(buffer.p);
    bool ret = true;
    if(!root) {
        const char *err = cJSON_GetErrorPtr();
        if(err)
            log_err("%s\n", err);
        ret = false;
        goto cleanup;
    }
    if(!get_next_batch(root, batch))
        ret = false;
    config_verbose(config, "next batch: %s\n", batch);
cleanup:
    cJSON_Delete(root);
    return ret;
}

bool get_next_batch(cJSON *j, char *batch) {
    const cJSON *b = get_item(j, "next_batch");
    if(!cJSON_IsString(b)) {
        log_err("\"next_batch\" not found or not string\n");
        return false;
    }
    size_t len = strlen(b->valuestring);
    if(len >= MAX_BATCH) {
        log_err("len(next_batch) >= MAX_BATCH\n");
        return false;
    }
    strcpy(batch, b->valuestring);
    return true;
}

bool filter(const struct config *config, char *batch) {
    const size_t user_len = strlen(config->c.short_user);
    struct mtrix_buffer buffer = {0};
    cJSON *req = NULL;
    bool ret = false;
    for(;;) {
        if(getline(&buffer.p, &buffer.n, stdin) == -1) {
            if(ferror(stdin))
                log_errno("getline");
            else
                ret = true;
            break;
        }
        if(!(req = parse_json(buffer.p)))
            break;
        if(!get_next_batch(req, batch))
            break;
        if(!handle_request(config, req, user_len, print_msg))
            break;
    }
    free(buffer.p);
    cJSON_Delete(req);
    return ret;
}

bool loop(const struct config *config, char *batch) {
    char url[MTRIX_MAX_URL_LEN];
    const char *const root = SYNC_URL "?" TIMEOUT_PARAM "&since=";
    const char *const url_parts[] =
        {URL_PARTS(&config->c, root, batch, "&"), NULL};
    if(!build_url(url, url_parts))
        return false;
    const size_t user_len = strlen(config->c.short_user);
    struct mtrix_buffer buffer = {0};
    cJSON *req = NULL;
    for(;;) {
        const time_t start = time(NULL);
        if(!build_url(url, url_parts))
            return false;
        buffer.n = 0;
        if(!request(url, &buffer, mtrix_config_verbose(&config->c)))
            break;
        if(!(req = parse_json(buffer.p)))
            break;
        if(!get_next_batch(req, batch))
            break;
        if(!handle_request(config, req, user_len, send_msg))
            break;
        const time_t dt = time(NULL) - start;
        config_verbose(config, "elapsed: %lds\n", dt);
    }
    free(buffer.p);
    cJSON_Delete(req);
    return false;
}

bool handle_request(
    const struct config *config, cJSON *root, size_t user_len,
    bool (*send_msg)(const struct config*, const char*, const char*)
) {
    bool ret = true;
    const cJSON *const join = get_item(get_item(root, "rooms"), "join");
    const cJSON *room = NULL;
    cJSON_ArrayForEach(room, join) {
        const cJSON *events = get_item(get_item(room, "timeline"), "events");
        const cJSON *event = NULL;
        cJSON_ArrayForEach(event, events) {
            if(!check_event_type(event, "m.room.message"))
                continue;
            const char *const text = event_body(event);
            if(!text)
                continue;
            const char *const sender = event_sender(event);
            if(!sender) {
                config_verbose(
                    config, "skipping message without sender: %s\n", text);
                continue;
            }
            config_verbose(config, "message (from %s): %s\n", sender, text);
            if(strcmp(sender, config->c.user) == 0) {
                config_verbose(config, "skipping message from self\n");
                continue;
            }
            if(!check_mention(text, config->c.short_user)) {
                config_verbose(config, "skipping message: not mentioned\n");
                continue;
            }
            struct mtrix_buffer output = {0};
            if(!process_input(config, text + user_len + 1, &output)) {
                ret = false;
                continue;
            }
            ret = send_msg(config, room->string, output.p) && ret;
            free(output.p);
        }
    }
    return true;
}

cJSON *parse_json(const char *s) {
    cJSON *const j = cJSON_Parse(s);
    if(!j) {
        const char *err = cJSON_GetErrorPtr();
        if(err)
            log_err("%s\n", err);
    }
    return j;
}

bool check_event_type(const cJSON *event, const char *value) {
    const cJSON *const t = get_item(event, "type");
    return cJSON_IsString(t) && strcmp(t->valuestring, value) == 0;
}

const char *event_body(const cJSON *event) {
    const cJSON *const content = get_item(event, "content");
    if(!content)
        return NULL;
    const cJSON *const type = get_item(content, "msgtype");
    if(!cJSON_IsString(type) || strcmp(type->valuestring, "m.text") != 0)
        return NULL;
    const cJSON *const body = get_item(content, "body");
    return (body && cJSON_IsString(body)) ? body->valuestring : NULL;
}

const char *event_sender(const cJSON *event) {
    const cJSON *const j = get_item(event, "sender");
    return (cJSON_IsString(j) && j->valuestring) ? j->valuestring : NULL;
}

bool check_mention(const char *text, const char *user) {
    const char *colon = is_prefix(user, text);
    return colon && *colon == ':';
}

bool process_input(
    const struct config *config, const char *input, struct mtrix_buffer *output
) {
    int in[2] = {-1, -1}, out[2] = {-1, -1}, err[2] = {-1, -1};
    FILE *child_in = NULL, *child_out = NULL, *child_err = NULL;
    struct mtrix_buffer msg = {0};
    bool ret = false;
    if(pipe(in) == -1) { log_errno("pipe"); goto cleanup; }
    if(pipe(out) == -1) { log_errno("pipe"); goto cleanup; }
    if(pipe(err) == -1) { log_errno("pipe"); goto cleanup; }
    pid_t pid = fork();
    if(pid == -1) {
        log_errno("fork");
        goto cleanup;
    }
    if(!pid) {
        close(in[1]);
        close(out[0]);
        close(err[0]);
        exit(!exec(config->args, in[0], out[1], err[1]));
    }
    close(in[0]);
    close(out[1]);
    close(err[1]);
    if(!(child_in = fdopen(in[1], "w"))) {
        log_errno("fdopen");
        goto cleanup;
    }
    if(!(child_out = fdopen(out[0], "r"))) {
        log_errno("fdopen");
        goto cleanup;
    }
    if(!(child_err = fdopen(err[0], "r"))) {
        log_errno("fdopen");
        goto cleanup;
    }
    if(!fputs(input, child_in)) {
        log_errno("fputs");
        goto cleanup;
    }
    if(fclose(child_in)) {
        log_errno("fclose");
        goto cleanup;
    }
    child_in = 0;
    in[1] = -1;
    if(!read_output(child_out, &msg))
        goto cleanup;
    if(!wait_n(1, &pid)) {
        free(msg.p);
        msg = (struct mtrix_buffer){0};
        const char err[] = "error: ";
        mtrix_buffer_append(err, 1, sizeof(err) - 1, &msg);
        if(!read_output(child_err, &msg))
            goto cleanup;
    }
    ret = true;
cleanup:
    if(child_in) {
        if(fclose(child_in)) {
            log_errno("fclose");
            ret = false;
        }
    } else if(in[1] != -1 && close(in[1]) == -1) {
        log_errno("close");
        ret = false;
    }
    if(child_out) {
        if(fclose(child_out)) {
            log_errno("fclose");
            ret = false;
        }
    } else if(out[0] != -1 && close(out[0]) == -1) {
        log_errno("close");
        ret = false;
    }
    if(child_err) {
        if(fclose(child_err)) {
            log_errno("fclose");
            ret = false;
        }
    } else if(err[0] != -1 && close(err[0]) == -1) {
        log_errno("close");
        ret = false;
    }
    *output = msg;
    return ret;
}

bool print_msg(const struct config *config, const char *room, const char *msg) {
    (void)config;
    printf("%s: %s", room, msg);
    return true;
}

bool send_msg(const struct config *config, const char *room, const char *msg) {
    char url[MTRIX_MAX_URL_LEN];
    if(!BUILD_MATRIX_URL(&config->c, url, ROOMS_URL "/", room, SEND_URL "?"))
        return false;
    cJSON *msg_json = cJSON_CreateObject();
    cJSON_AddItemToObject(msg_json, "msgtype", cJSON_CreateString("m.text"));
    cJSON_AddItemToObject(msg_json, "body", cJSON_CreateString(msg));
    char *data = cJSON_PrintUnformatted(msg_json);
    free(msg_json);
    struct mtrix_buffer resp = {0};
    const post_request r = {.url = url, .data_len = strlen(data), .data = data};
    const bool ret = post(r, mtrix_config_verbose(&config->c), &resp);
    free(data);
    free(resp.p);
    return ret;
}

bool read_output(FILE *f, struct mtrix_buffer *buf) {
    char buffer[1024];
    while(!feof(f)) {
        const size_t n = fread(buffer, 1, sizeof(buffer), f);
        if(ferror(f))
            return log_errno("%s: fread", __func__), false;
        mtrix_buffer_append(buffer, 1, n, buf);
    }
    return true;
}
