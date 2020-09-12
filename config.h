#include <stdbool.h>
#include <stddef.h>

#define MAX_SERVER ((size_t)256U)
#define MAX_USER ((size_t)256U)
#define MAX_TOKEN ((size_t)512U)
#define MAX_BATCH ((size_t)512U)

typedef struct {
    bool help, verbose, dry;
    char server[MAX_SERVER], user[MAX_USER], token[MAX_TOKEN], batch[MAX_BATCH];
} mtrix_config;

void init_config(mtrix_config *config);
