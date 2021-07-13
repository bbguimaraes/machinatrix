/**
 * \file
 * Utility functions.
 */
#include <stdbool.h>
#include <stdio.h>

/** Maximum path length, arbitrarily chosen. */
#define MTRIX_MAX_PATH ((size_t)1024U)

/** Maximum unix socket path length, based on Linux's maximum. */
#define MTRIX_MAX_UNIX_PATH ((size_t)108U)

/** Maximum length for URLs built by \ref build_url. */
#define MTRIX_MAX_URL_LEN ((size_t)1024U)

/** Maximum number of command arguments (excluding the command name). */
#define MTRIX_MAX_ARGS ((size_t)1U)

/** Helper macro for \ref build_url that creates a `const char *` array. */
#define BUILD_URL(url, ...) build_url(url, (const char *[]){__VA_ARGS__, NULL})

/** Program name, used as a prefix for log messages if non-`NULL`. */
extern const char *PROG_NAME;

/** Command name, used as a second prefix for log messages if non-`NULL`. */
extern const char *CMD_NAME;

/** Sets the output stream used by log functions and returns previous value. */
FILE *log_set(FILE *f);

/**
 * Writes the printf-style message to the error output.
 * If non-null, \ref PROG_NAME and \ref CMD_NAME are prepended.
 */
void log_err(const char *fmt, ...);

/** \see log_err */
void log_errv(const char *fmt, va_list argp);

/**
 * Similar to \ref log_err, but also logs `strerror(errno)`.
 * `: %s\n` is appended, where `%s` is the result of `strerror(errno)`.  `errno`
 * is cleared.
 */
void log_errno(const char *fmt, ...);

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

/** Performs an \c open(2)s with \c O_CREAT followed by \c fopen. */
FILE *open_or_create(const char *path, const char *flags);

/**
 * Repeatedly calls \c read(2) until all data are read.
 * \returns \c true iff \c n bytes were read without error.
 */
bool read_all(int fd, void *p, size_t n);

/**
 * Repeatedly calls \c write(2) until all data are written.
 * \returns \c true iff \c n bytes were written without error.
 */
bool write_all(int fd, const void *p, size_t n);

/**
 * Executes a command with optional input/output/error redirection.
 * If any of the `f*` parameters are not `-1`, the corresponding file descriptor
 * is replaced by it before the `exec` call.
 * \param argv `execv`-style argument list.
 * \param fin Substitute for `stdin`.
 * \param fout Substitute for `stdout`.
 * \param ferr Substitute for `stderr`.
 */
bool exec(const char *const *argv, int fin, int fout, int ferr);

/** Waits for `n` child processes to exit. */
bool wait_n(size_t n);

/** Replaces new-line characters with spaces. */
void join_lines(unsigned char *b, unsigned char *e);

/** Resizable buffer used by several functions. */
typedef struct {
    /** Owning pointer to the dynamically-allocated data. */
    char *p;
    /** Size of the buffer pointed to by \ref p. */
    size_t s;
} mtrix_buffer;

/** Copies data to the buffer, reallocating if necessary. */
size_t mtrix_buffer_append(
    const char *p, size_t size, size_t n, mtrix_buffer *b);

/** Joins several URL parts into one, limited to \ref MTRIX_MAX_URL_LEN. */
bool build_url(char *url, const char *const *v);

/**
 * Performs a `GET` request.
 * \param url Target RUL.
 * \param b Output buffer, resized as required.
 * \param verbose Emit debug output.
 */
bool request(const char *url, mtrix_buffer *b, bool verbose);

/** Data used for `POST` requests. */
typedef struct {
    /** Target URL. */
    const char *url;
    /** Length of \ref data. */
    size_t data_len;
    /** `POST` payload. */
    const char *data;
} post_request;

/**
 * Performs a `POST` request.
 * \param r `POST` url/payload.
 * \param b Output buffer, resized as required.
 * \param verbose Emit debug output.
 */
bool post(post_request r, bool verbose, mtrix_buffer *b);
