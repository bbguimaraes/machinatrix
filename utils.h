#include <stdio.h>
#include <stdbool.h>

#define MTRIX_MAX_URL_LEN 1024

extern const char *PROG_NAME;
extern const char *CMD_NAME;

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
bool build_url(char *url, const char **v);

bool request(const char *url, mtrix_buffer *b, bool verbose);
bool post(
    const char *url, size_t n, const char *data,
    mtrix_buffer *b, bool verbose);