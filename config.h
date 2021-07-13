/**
 * \file
 * Configuration structure used by all programs and associated functions.
 */
#include <stdbool.h>
#include <stddef.h>

/**
 * Maximum length for the server URL (including null terminator).
 * https://matrix.org/docs/spec/appendices#server-name
 */
#define MAX_SERVER ((size_t)231U)

/**
 * Maximum length for the user name (including null terminator).
 *
 * https://matrix.org/docs/spec/appendices#user-identifiers
 */
#define MAX_USER ((size_t)256U)

/**
 * Maximum length for the access token (including null terminator).
 * Arbitrarily set, `synapse` currently generates ~300-byte tokens.
 */
#define MAX_TOKEN ((size_t)512U)

/**
 * Maximum size for the batch identifier (including null terminator).
 * Arbitrarily set, `synapse` currently generates ~50-byte identifiers.
 */
#define MAX_BATCH ((size_t)512U)

/**
 * Configuration structure used by all programs.
 * Can be safely zero-initialized.
 */
struct mtrix_config {
    /** Whether the `help` command was requested. */
    bool help;
    /** Whether verbose logging was requested. */
    bool verbose;
    /** Whether dry-run mode was requested. */
    bool dry;
    /** Matrix server URL. */
    char server[MAX_SERVER];
    /** Matrix access token. */
    char token[MAX_TOKEN];
    /** User name used to send messages, in the form `@username:server`. */
    char user[MAX_USER];
    /** Short version of the user name, stripped of the `@` sign and server. */
    char short_user[MAX_USER];
    /** Last batch received from the Matrix server. */
    char batch[MAX_BATCH];
};
