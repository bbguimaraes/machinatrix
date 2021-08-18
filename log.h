#include <stdarg.h>
#include <stdio.h>

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
