#include "config.h"

void init_config(mtrix_config *config) {
    config->help = false;
    config->verbose = false;
    config->dry = false;
    config->server[0] = '\0';
    config->user[0] = '\0';
    config->token[0] = '\0';
    config->batch[0] = '\0';
}
