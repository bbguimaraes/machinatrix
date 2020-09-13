/**
 * \file
 * Utility functions.
 */
#include <stdbool.h>
#include <stdio.h>

/**
 * Maximum length for URLs built by \ref build_url.
 */
#define MTRIX_MAX_URL_LEN ((size_t)1024U)

/**
 * Helper macro for \ref build_url that creates a `const char *` array.
 */
#define BUILD_URL(url, ...) build_url(url, (const char *[]){__VA_ARGS__, NULL})

/**
 * Program name, used as a prefix for log messages if non-`NULL`.
 */
extern const char *PROG_NAME;

/**
 * Command name, used as a second prefix for log messages if non-`NULL`.
 */
extern const char *CMD_NAME;

/**
 * Sets the output stream used by log functions and returns previous value.
 */
FILE *log_set(FILE *f);

/**
 * Writes the printf-style message to the error output.
 * If non-null, \ref PROG_NAME and \ref CMD_NAME are prepended.
 */
void log_err(const char *fmt, ...);

/**
 * Checks if a string has a certain prefix.
 * \return The first character after the prefix or `NULL` if not a prefix.
 */
char *is_prefix(const char *prefix, const char *s);

/**
 * Copies a value from an `argv`-style array.
 * Checks for non-emptiness and length are performed and errors are logged.
 */
bool copy_arg(const char *name, char *dst, const char *src, size_t max);

/**
 * Calls the corresponding system call, with optional input/output FDs.
 * \param argv `execv`-style argument list.
 * \param fin Substitute for `stdin`.
 * \param fout Substitute for `stdout`.
 */
bool exec(const char *const *argv, int fin, int fout);

/**
 * Waits for `n` child processes to exit.
 */
bool wait_n(size_t n);

/**
 * Replaces new-line characters with spaces.
 */
void join_lines(unsigned char *b, unsigned char *e);

/**
 * Resizable buffer used by several functions.
 */
typedef struct {
    /**
     * Owning pointer to the dynamically-allocated data.
     */
    char *p;
    /**
     * Size of the buffer pointed to by \ref p.
     */
    size_t s;
} mtrix_buffer;

/**
 * Copies data to the buffer, reallocating if necessary.
 */
size_t mtrix_buffer_append(char *p, size_t size, size_t n, mtrix_buffer *b);

/**
 * Joins several URL parts into one, limited to \ref MTRIX_MAX_URL_LEN.
 */
bool build_url(char *url, const char *const *v);

/**
 * Performs a `GET` request.
 * \param url Target RUL.
 * \param b Output buffer, resized as required.
 * \param verbose Emit debug output.
 */
bool request(const char *url, mtrix_buffer *b, bool verbose);

/**
 * Data used for `POST` requests.
 */
typedef struct {
    /**
     * Target URL.
     */
    const char *url;
    /**
     * Length of \ref data.
     */
    size_t data_len;
    /**
     * `POST` payload.
     */
    const char *data;
} post_request;

/**
 * Performs a `POST` request.
 * \param r `POST` url/payload.
 * \param b Output buffer, resized as required.
 * \param verbose Emit debug output.
 */
bool post(post_request r, bool verbose, mtrix_buffer *b);
