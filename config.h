#include <stdbool.h>

typedef struct {
    bool help, verbose, dry;
} mtrix_config;

void init_config(mtrix_config *config);
