#include <stdbool.h>
#include <stddef.h>

#define MAX_SERVER ((size_t)256U)
#define MAX_USER ((size_t)256U)
#define MAX_TOKEN ((size_t)512U)
#define MAX_BATCH ((size_t)512U)

typedef struct {
    bool help, verbose, dry;
    char server[MAX_SERVER], token[MAX_TOKEN];
    char user[MAX_USER], short_user[MAX_USER], batch[MAX_BATCH];
} mtrix_config;
