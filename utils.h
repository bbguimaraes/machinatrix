#include <stdbool.h>
#include <stdio.h>

#define MTRIX_MAX_URL_LEN ((size_t)1024U)
#define BUILD_URL(url, ...) build_url(url, (const char *[]){__VA_ARGS__, NULL})

extern const char *PROG_NAME;
extern const char *CMD_NAME;

/**
 * log_set sets the output file stream used by log functions.
 * The previous value is returned.
 */
FILE *log_set(FILE *f);

/**
 * log_err writes the printf-style message to the error output.
 * If non-null, PROG_NAME and CMD_NAME are prepended.
 */
void log_err(const char *fmt, ...);

/**
 * copy_arg copies a value from an argv-style array.
 * Checks for non-emptiness and length are performed and errors are logged.
 */
bool copy_arg(const char *name, char *dst, const char *src, size_t max);

/**
 * exec calls the corresponding system call, with optional input/output FDs.
 */
bool exec(const char *const *argv, int fin, int fout);

/**
 * wait_n waits for the specified number of child processes.
 */
bool wait_n(size_t n);

/**
 * join_lines replaces new-line characters with spaces.
 */
void join_lines(unsigned char *b, unsigned char *e);

typedef struct { char *p; size_t s; } mtrix_buffer;

/**
 * mtrix_buffer_append copies data to the buffer, reallocating if necessary.
 */
size_t mtrix_buffer_append(char *in, size_t size, size_t n, mtrix_buffer *b);

/**
 * build_url joins several URL parts into one, limited to MTRIX_MAX_URL_LEN.
 */
bool build_url(char *url, const char *const *v);

bool request(const char *url, mtrix_buffer *b, bool verbose);

typedef struct {
    const char *url;
    size_t data_len;
    const char *data;
} post_request;

bool post(post_request r, bool verbose, mtrix_buffer *b);
