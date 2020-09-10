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
const char *PROG_NAME = NULL, *CMD_NAME = NULL;

int main(int argc, char *const *argv);
bool parse_args(int *argc, char *const **argv, mtrix_config *config);
void usage(FILE *f);
bool init_batch(const mtrix_config *config, char *batch);
bool get_next_batch(cJSON *j, char *batch);
bool loop(const mtrix_config *config, char *batch);
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
        [HELP]    = {"help",    no_argument,       0, 'h'},
        [VERBOSE] = {"verbose", no_argument,       0, 'v'},
        [DRY]     = {"dry-run", no_argument,       0, 'n'},
        [SERVER]  = {"server",  required_argument, 0,  0 },
        [USER]    = {"user",    required_argument, 0,  0 },
        [TOKEN]   = {"token",   required_argument, 0,  0 },
        [BATCH]   = {"batch",   required_argument, 0,  0 },
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
    fprintf(f,
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

bool init_batch(const mtrix_config *config, char *batch) {
    // TODO use Authorization header
    const char *url_parts[] = {
        config->server,
        "/_matrix/client/r0/sync"
            "?filter={\"room\":{\"timeline\":{\"limit\":1}}}"
            "&access_token=",
        config->token, NULL};
    char url[MTRIX_MAX_URL_LEN];
    if(!build_url(url, url_parts))
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
    const cJSON *b = cJSON_GetObjectItemCaseSensitive(j, "next_batch");
    if(!b || !cJSON_IsString(b)) {
        log_err("'next_batch' not found or not string\n");
        return false;
    }
    size_t len = strlen(b->valuestring);
    if(len >= MAX_BATCH) {
        log_err("len('next_batch') >= MAX_BATCH\n");
        return false;
    }
    strcpy(batch, b->valuestring);
    return true;
}

bool loop(const mtrix_config *config, char *batch) {
    const char *username = config->user + 1;
    size_t username_len = strchr(username, ':') - username;
    const char *url_parts[] = {
        config->server, "/_matrix/client/r0/sync?since=", batch,
        "&timeout=" SYNC_INTERVAL_MS_STR
        "&access_token=", config->token, NULL};
    char url[MTRIX_MAX_URL_LEN];
    mtrix_buffer buffer = {NULL, 0};
    cJSON *root = NULL;
    for(;;) {
        const time_t start = time(NULL);
        if(!build_url(url, url_parts))
            return false;
        buffer.s = 0;
        bool ret = request(url, &buffer, config->verbose);
        if(!ret)
            break;
        root = cJSON_Parse(buffer.p);
        if(!root) {
            ret = false;
            const char *err = cJSON_GetErrorPtr();
            if(err)
                log_err("%s\n", err);
            break;
        }
        if(!get_next_batch(root, batch)) {
            ret = false;
            break;
        }
        const cJSON *const rooms =
            cJSON_GetObjectItemCaseSensitive(
                cJSON_GetObjectItemCaseSensitive(root, "rooms"),
                "join");
        const cJSON *room = NULL;
        cJSON_ArrayForEach(room, rooms) {
            const cJSON *events =
                cJSON_GetObjectItemCaseSensitive(
                    cJSON_GetObjectItemCaseSensitive(room, "timeline"),
                    "events");
            const cJSON *event = NULL;
            cJSON_ArrayForEach(event, events) {
                const cJSON *type =
                    cJSON_GetObjectItemCaseSensitive(event, "type");
                if(!type || !cJSON_IsString(type)
                        || strcmp(type->valuestring, "m.room.message"))
                    continue;
                const cJSON *const content =
                    cJSON_GetObjectItemCaseSensitive(event, "content");
                const cJSON *const msg_type =
                    cJSON_GetObjectItemCaseSensitive(content, "msgtype");
                if(!msg_type || !cJSON_IsString(msg_type)
                        || strcmp(msg_type->valuestring, "m.text"))
                    continue;
                const cJSON *const text =
                    cJSON_GetObjectItemCaseSensitive(content, "body");
                if(!text || !cJSON_IsString(text))
                    continue;
                const cJSON *const sender =
                    cJSON_GetObjectItemCaseSensitive(event, "sender");
                if(!sender || !cJSON_IsString(sender)
                        || !strcmp(sender->valuestring, config->user))
                    continue;
                if(config->verbose)
                    printf(
                        "Message (from %s): %s\n",
                        sender->valuestring, text->valuestring);
                if(strncmp(text->valuestring, username, username_len)
                        || *(text->valuestring + username_len) != ':') {
                    if(config->verbose)
                        printf(
                            "Skipping: '%.*s' != '%.*s:'\n",
                            (int)username_len + 1, text->valuestring,
                            (int)username_len, username);
                    continue;
                }
                if(!reply(
                        config, room->string,
                        text->valuestring + username_len + 1))
                    ; // TODO
            }
        }
        const time_t dt = time(NULL) - start;
        if(config->verbose)
            printf("Elapsed: %lds\n", dt);
    }
    free(buffer.p);
    cJSON_Delete(root);
    return true;
}

bool reply(const mtrix_config *config, const char *room, const char *input) {
    int in[2] = {0, 0}, out[2] = {0, 0};
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
    FILE *child_in = fdopen(in[1], "w"), *child_out = fdopen(out[0], "r");
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
    mtrix_buffer msg = {NULL, 0};
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
    const char *url_parts[] = {
        config->server,
        "/_matrix/client/r0/rooms/", room,
        "/send/m.room.message?access_token=",
        config->token, NULL};
    char url[MTRIX_MAX_URL_LEN];
    if(!build_url(url, url_parts))
        return false;
    cJSON *msg_json = cJSON_CreateObject();
    cJSON_AddItemToObject(msg_json, "msgtype", cJSON_CreateString("m.text"));
    cJSON_AddItemToObject(msg_json, "body", cJSON_CreateString(msg));
    char *msg_str = cJSON_PrintUnformatted(msg_json);
    free(msg_json);
    mtrix_buffer resp = {NULL, 0};
    const bool ret =
        post(url, strlen(msg_str), msg_str, &resp, config->verbose);
    free(msg_str);
    free(resp.p);
    return ret;
}
