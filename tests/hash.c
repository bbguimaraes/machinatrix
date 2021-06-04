#include "hash.h"

#include "tests/common.h"

static bool test_init(void) {
    return ASSERT_EQ(MTRIX_HASHER_INIT.h, 0x1505);
}

static bool test_hash_add(void) {
    const struct mtrix_hasher h = mtrix_hasher_add(MTRIX_HASHER_INIT, 42);
    return ASSERT_EQ(h.h, 0x2b5cf);
}

static bool test_hash_str(void) {
    const struct mtrix_hasher h =
        mtrix_hasher_add_str(MTRIX_HASHER_INIT, "str");
    return ASSERT_EQ(mtrix_hash_str("str"), h.h);
}

static bool test_hash_add_str(void) {
    const struct mtrix_hasher h =
        mtrix_hasher_add_str(MTRIX_HASHER_INIT, "str");
    return ASSERT_EQ(h.h, 0xb88ab7e);
}

static bool test_hash_add_bytes(void) {
    const char s[] = "42";
    const struct mtrix_hasher h =
        mtrix_hasher_add_bytes(MTRIX_HASHER_INIT, s, sizeof(s) - 1);
    return ASSERT_EQ(h.h, 0x59712b);
}

int main(void) {
    bool ret = true;
    ret = RUN(test_init) && ret;
    ret = RUN(test_hash_add) && ret;
    ret = RUN(test_hash_str) && ret;
    ret = RUN(test_hash_add_str) && ret;
    ret = RUN(test_hash_add_bytes) && ret;
    return !ret;
}
