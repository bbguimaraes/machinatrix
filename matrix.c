#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cjson/cJSON.h>
#include <getopt.h>
#include <unistd.h>

#include "config.h"
#include "utils.h"

#define SYNC_INTERVAL_S ((size_t)30U)
#define SYNC_INTERVAL_MS_STR "30000"
#define MIN_SYNC_INTERVAL_S ((size_t)5U)
#define API_URL "/_matrix/client/r0"
#define SYNC_URL API_URL "/sync"
#define ROOM_FILTER "filter={\"room\":{\"timeline\":{\"limit\":1}}}"
#define ROOMS_URL API_URL "/rooms"
#define SEND_URL "/send/m.room.message"
#define TIMEOUT_PARAM "timeout=" SYNC_INTERVAL_MS_STR
#define URL_PARTS(c, ...) c->server, __VA_ARGS__, "access_token=", c->token
#define BUILD_MATRIX_URL(c, url, ...) \
    BUILD_URL(url, URL_PARTS(config, __VA_ARGS__))

const char *PROG_NAME = NULL, *CMD_NAME = NULL;

int main(int argc, char *const *argv);
bool parse_args(int *argc, char *const **argv, mtrix_config *config);
void usage(FILE *f);
cJSON *get_item(const cJSON *j, const char *k);
bool initial_sync_url(const mtrix_config *config, char *url);
bool send_url(const mtrix_config *config, char *url, const char *room);
bool init_batch(const mtrix_config *config, char *batch);
bool get_next_batch(cJSON *j, char *batch);
bool loop(const mtrix_config *config, char *batch);
cJSON *parse_request(const char *r);
void handle_request(const mtrix_config *config, cJSON *root, size_t user_len);
bool check_event_type(const cJSON *event, const char *value);
const char *event_body(const cJSON *event);
const char *event_sender(const cJSON *event, const char *username);
bool check_mention(const char *text, size_t user_len, const char *user);
bool reply(const mtrix_config *config, const char *room, const char *input);
bool send_msg(const mtrix_config *config, const char *room, const char *msg);

int main(int argc, char *const *argv) {
    log_set(stderr);
    PROG_NAME = argv[0];
    mtrix_config config;
    init_config(&config);
    if(!parse_args(&argc, &argv, &config))
        return 1;
    if(config.help) {
        usage(stdout);
        return 0;
    }
    if(!*config.server) {
        log_err("no server specified\n");
        return 1;
    }
    if(!*config.user) {
        log_err("no user specified\n");
        return 1;
    }
    if(!*config.token) {
        log_err("no token specified\n");
        return 1;
    }
    if(config.verbose) {
        printf("Using server: %s\n", config.server);
        printf("Using user: %s\n", config.user);
    }
    char batch[MAX_BATCH];
    if(*config.batch) {
        assert(strlen(config.batch) < sizeof(batch));
        strcpy(batch, config.batch);
    } else if(!init_batch(&config, batch))
        return 1;
    if(config.verbose)
        printf("Using batch: %s\n", batch);
    return !loop(&config, batch);
}

bool parse_args(int *argc, char *const **argv, mtrix_config *config) {
    enum { HELP, VERBOSE, DRY, SERVER, USER, TOKEN, BATCH };
    static const char *short_opts = "hvn";
    static const struct option long_opts[] = {
        [HELP] = {"help", no_argument, 0, 'h'},
        [VERBOSE] = {"verbose", no_argument, 0, 'v'},
        [DRY] = {"dry-run", no_argument, 0, 'n'},
        [SERVER] = {"server", required_argument, 0, 0},
        [USER] = {"user", required_argument, 0, 0},
        [TOKEN] = {"token", required_argument, 0, 0},
        [BATCH] = {"batch", required_argument, 0, 0},
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
        case 0: break;
        default: return false;
        }
        switch(idx) {
        case SERVER:
            if(!copy_arg("server name", config->server, optarg, MAX_SERVER))
                return false;
            break;
        case USER:
            if(!copy_arg("user name", config->user, optarg, MAX_USER))
                return false;
            break;
        case TOKEN:
            // TODO read from file
            if(!copy_arg("token", config->token, optarg, MAX_TOKEN))
                return false;
            break;
        case BATCH:
            if(!copy_arg("batch", config->batch, optarg, MAX_BATCH))
                return false;
            break;
        default: return false;
        }
    }
    *argv += optind;
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
        "        --batch  <arg>     matrix batch file\n",
        PROG_NAME);
}

inline cJSON *get_item(const cJSON *j, const char *k) {
    return cJSON_GetObjectItemCaseSensitive(j, k);
}

// TODO use Authorization header
bool initial_sync_url(const mtrix_config *config, char *url) {
    return BUILD_MATRIX_URL(config, url, SYNC_URL "?" ROOM_FILTER "&");
}

bool send_url(const mtrix_config *config, char *url, const char *room) {
    return BUILD_MATRIX_URL(config, url, ROOMS_URL "/", room, SEND_URL "?");
}

bool init_batch(const mtrix_config *config, char *batch) {
    char url[MTRIX_MAX_URL_LEN];
    if(!initial_sync_url(config, url))
        return false;
    mtrix_buffer buffer = {NULL, 0};
    if(!request(url, &buffer, config->verbose)) {
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
    if(config->verbose)
        printf("Next batch: %s\n", batch);
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

bool loop(const mtrix_config *config, char *batch) {
    char url[MTRIX_MAX_URL_LEN];
    const char *const root = SYNC_URL "?" TIMEOUT_PARAM "&since=";
    const char *const url_parts[] = {URL_PARTS(config, root, batch, "&"), NULL};
    if(!build_url(url, url_parts))
        return false;
    const char *username = config->user + 1;
    size_t user_len = strchr(username, ':') - username;
    mtrix_buffer buffer = {0};
    cJSON *req = NULL;
    for(;;) {
        const time_t start = time(NULL);
        if(!build_url(url, url_parts))
            return false;
        buffer.s = 0;
        if(!request(url, &buffer, config->verbose))
            break;
        if(!(req = parse_request(buffer.p)))
            break;
        if(!get_next_batch(req, batch))
            break;
        handle_request(config, req, user_len);
        const time_t dt = time(NULL) - start;
        if(config->verbose)
            printf("Elapsed: %lds\n", dt);
    }
    free(buffer.p);
    cJSON_Delete(req);
    return false;
}

void handle_request(const mtrix_config *config, cJSON *root, size_t user_len) {
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
            const char *const sender = event_sender(event, config->user);
            if(config->verbose)
                printf("Message (from %s): %s\n", sender, text);
            if(!check_mention(text, user_len, config->user)) {
                if(config->verbose)
                    printf("Skipping message: not mentioned\n");
                continue;
            }
            if(!reply(config, room->string, text + user_len + 1))
                ; // TODO
        }
    }
}

cJSON *parse_request(const char *r) {
    cJSON *const j = cJSON_Parse(r);
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

const char *event_sender(const cJSON *event, const char *username) {
    const cJSON *const j = get_item(event, "sender");
    return (cJSON_IsString(j) && strcmp(j->valuestring, username) != 0)
        ? j->valuestring
        : NULL;
}

bool check_mention(const char *text, size_t user_len, const char *user) {
    return strncmp(text, user, user_len) == 0 && text[user_len] == ':';
}

bool reply(const mtrix_config *config, const char *room, const char *input) {
    int in[2] = {0, 0}, out[2] = {0, 0};
    FILE *child_in = NULL, *child_out = NULL;
    mtrix_buffer msg = {NULL, 0};
    bool ret = true;
    if(pipe(in) == -1 || pipe(out) == -1) {
        ret = false;
        log_err("pipe: %s\n", strerror(errno));
        goto cleanup;
    }
    pid_t pid = fork();
    if(pid == -1) {
        ret = false;
        log_err("fork: %s\n", strerror(errno));
        goto cleanup;
    }
    if(!pid) {
        close(in[1]);
        close(out[0]);
        const char *cargv[] = {"./machinatrix", 0};
        return exec(cargv, in[0], out[1]);
    }
    close(in[0]);
    close(out[1]);
    child_in = fdopen(in[1], "w");
    child_out = fdopen(out[0], "r");
    if(!child_in || !child_out) {
        ret = false;
        log_err("fdopen: %s\n", strerror(errno));
        goto cleanup;
    }
    if(!fputs(input, child_in)) {
        ret = false;
        log_err("fputs: %s\n", strerror(errno));
        goto cleanup;
    }
    fclose(child_in);
    child_in = 0;
    char *buffer = 0;
    for(;;) {
        size_t len;
        if((len = getline(&buffer, &len, child_out)) == -1)
            break;
        mtrix_buffer_append(buffer, 1, len, &msg);
    }
    if(buffer)
        free(buffer);
    if(!wait_n(1)) {
        ret = false;
        goto cleanup;
    }
    if(!send_msg(config, room, msg.p))
        ret = false;
cleanup:
    if(child_in)
        fclose(child_in);
    else if(in[0])
        close(in[0]);
    if(child_out)
        fclose(child_out);
    else if(out[0])
        close(out[0]);
    free(msg.p);
    return ret;
}

bool send_msg(const mtrix_config *config, const char *room, const char *msg) {
    char url[MTRIX_MAX_URL_LEN];
    if(!send_url(config, url, room))
        return false;
    cJSON *msg_json = cJSON_CreateObject();
    cJSON_AddItemToObject(msg_json, "msgtype", cJSON_CreateString("m.text"));
    cJSON_AddItemToObject(msg_json, "body", cJSON_CreateString(msg));
    char *data = cJSON_PrintUnformatted(msg_json);
    free(msg_json);
    mtrix_buffer resp = {NULL, 0};
    const post_request r = {.url = url, .data_len = strlen(data), .data = data};
    const bool ret = post(r, config->verbose, &resp);
    free(data);
    free(resp.p);
    return ret;
}
