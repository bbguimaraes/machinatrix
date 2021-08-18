/**
 * \file
 * Utility functions.
 */
#include <stdbool.h>
#include <stdio.h>

#include <sys/types.h>

/** Maximum path length, arbitrarily chosen. */
#define MTRIX_MAX_PATH ((size_t)1024U)

/** Maximum unix socket path length, based on Linux's maximum. */
#define MTRIX_MAX_UNIX_PATH ((size_t)108U)

/** Maximum length for URLs built by \ref build_url. */
#define MTRIX_MAX_URL_LEN ((size_t)1024U)

/** Maximum number of command arguments (excluding the command name). */
#define MTRIX_MAX_ARGS ((size_t)2U)

/** Helper macro for \ref build_url that creates a `const char *` array. */
#define BUILD_URL(url, ...) build_url(url, (const char *[]){__VA_ARGS__, NULL})

/** Resizable buffer used by several functions. */
struct mtrix_buffer {
    /** Owning pointer to the dynamically-allocated data. */
    char *p;
    /** Size of the buffer pointed to by \ref p. */
    size_t n;
};

/**

/** See glibc's function. */
static const char *strchrnul(const char *s, int c);

/** See libbsd's function. */
static size_t strlcpy(char *restrict dst, const char *restrict src, size_t n);

/**
 * Checks if a string has a certain prefix.
 * \return The first character after the prefix or `NULL` if not a prefix.
 */
char *is_prefix(const char *prefix, const char *s);

/**
 * Copies a value from an `argv`-style array.
 * Checks for non-emptiness and length are performed and errors are logged.
 */
bool copy_arg(const char *name, struct mtrix_buffer dst, const char *src);

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
bool wait_n(size_t n, const pid_t *p);

/** Replaces new-line characters with spaces. */
void join_lines(unsigned char *b, unsigned char *e);

/** Copies data to the buffer, reallocating if necessary. */
size_t mtrix_buffer_append(
    const char *p, size_t size, size_t n, struct mtrix_buffer *b);

/** Joins several URL parts into one, limited to \ref MTRIX_MAX_URL_LEN. */
bool build_url(char *url, const char *const *v);

/**
 * Performs a `GET` request.
 * \param url Target RUL.
 * \param b Output buffer, resized as required.
 * \param verbose Emit debug output.
 */
bool request(const char *url, struct mtrix_buffer *b, bool verbose);

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
bool post(post_request r, bool verbose, struct mtrix_buffer *b);

static inline const char *strchrnul(const char *s, int c) {
    while(*s && *s != c)
        ++s;
    return s;
}

static inline size_t strlcpy(
    char *restrict dst, const char *restrict src, size_t n)
{
    const char *const p = dst;
    while(n && (*dst++ = *src++))
        --n;
    return (size_t)(dst - p);
}
