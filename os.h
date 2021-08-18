/**
 * \file
 * Operating system functions.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/** Concatenate \c n segments, with length checking. */
char *join_path(char v[static MTRIX_MAX_PATH], int n, ...);

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

/** \see read_all */
bool fread_all(FILE *f, void *p, size_t n);

/** \see write_all */
bool fwrite_all(FILE *f, const void *p, size_t n);
