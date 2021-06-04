#ifndef MACHINATRIX_HASH_H
#define MACHINATRIX_HASH_H

#include <stddef.h>
#include <stdint.h>

#define MTRIX_HASHER_INIT ((struct mtrix_hasher){.h = 0x1505})

typedef uint64_t mtrix_hash;

struct mtrix_hasher {
    mtrix_hash h;
};

/** Hashes \c b and combines it with the existing state of \c h. */
static inline struct mtrix_hasher mtrix_hasher_add(
    struct mtrix_hasher h, char b);

/** Hashes \c s and combines it with the existing state of \c h. */
static inline struct mtrix_hasher mtrix_hasher_add_str(
    struct mtrix_hasher h, const char *s);

/** Hashes \c p and combines it with the existing state of \c h. */
static inline struct mtrix_hasher mtrix_hasher_add_bytes(
    struct mtrix_hasher h, const void *p, size_t n);

/** Equivalent to <tt>hasher_add_str(MTRIX_HASHER_INIT, s)</tt>. */
static inline mtrix_hash mtrix_hash_str(const char *s);

struct mtrix_hasher mtrix_hasher_add(struct mtrix_hasher h, char b) {
    return (struct mtrix_hasher){.h = ((h.h << 5) + h.h) + (uint64_t)b};
}

struct mtrix_hasher mtrix_hasher_add_str(
    struct mtrix_hasher h, const char *s
) {
    for(char c; (c = *s++);)
        h = mtrix_hasher_add(h, c);
    return h;
}

struct mtrix_hasher mtrix_hasher_add_bytes(
    struct mtrix_hasher h, const void *vp, size_t n
) {
    for(const char *p = vp; n--;)
        h = mtrix_hasher_add(h, *p++);
    return h;
}

mtrix_hash mtrix_hash_str(const char *s) {
    return mtrix_hasher_add_str(MTRIX_HASHER_INIT, s).h;
}

#endif
