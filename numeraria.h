#ifndef MACHINATRIX_NUMERARIA_H
#define MACHINATRIX_NUMERARIA_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/** Format of each command packet. */
struct mtrix_numeraria_cmd {
    /** Length of the packet's data section. */
    uint32_t len;
    /** One of the \c MTRIX_NUMERARIA_CMD_* constants. */
    uint8_t cmd;
    /** Variable-length data.  \see len */
    char data[];
};

enum {
    /** Maximum command length. */
    MTRIX_NUMERARIA_MAX_CMD = 1024,
    /** Size of the command header. */
    MTRIX_NUMERARIA_CMD_SIZE = offsetof(struct mtrix_numeraria_cmd, data),
    /** Maximum size of a packet. */
    MTRIX_NUMERARIA_MAX_PACKET =
        MTRIX_NUMERARIA_MAX_CMD + MTRIX_NUMERARIA_CMD_SIZE,
    MTRIX_NUMERARIA_CMD_EXIT = 1,
    MTRIX_NUMERARIA_CMD_SQL = 2,
    MTRIX_NUMERARIA_CMD_RECORD_CMD = 3,
    MTRIX_NUMERARIA_CMD_STATS = 4,
};

static_assert(
    MTRIX_NUMERARIA_CMD_SIZE
    == sizeof(((struct mtrix_numeraria_cmd*)0)->len)
        + sizeof(((struct mtrix_numeraria_cmd*)0)->cmd));

/**
 * Creates a TCP socket connection with numeraria.
 * \returns: fd or -1.
 */
int mtrix_numeraria_init_socket(const char *addr);

/**
 * Creates a Unix socket connection with numeraria.
 * \returns: fd or -1.
 */
int mtrix_numeraria_init_unix(const char *path);

#endif
