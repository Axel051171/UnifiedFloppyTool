/**
 * @file test_d81_plugin.c
 * @brief D81 (Commodore 1581) Plugin — probe tests.
 *
 * Self-contained per template from test_d64_plugin.c.
 * Mirrors src/formats/commodore/d81.c probe logic (size-based match).
 *
 * D81 is Commodore's 3.5" MFM disk format: 80 tracks × 2 sides ×
 * 10 sectors × 512 bytes = 819200 bytes.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-28s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D81_SIZE   819200u  /* 80 × 2 × 10 × 512 */

static bool d81_probe_replica(size_t file_size) {
    return file_size == D81_SIZE;
}

TEST(probe_exact_size) {
    ASSERT(d81_probe_replica(D81_SIZE));
}

TEST(probe_arithmetic_check) {
    /* D81 = 80 tracks × 2 sides × 10 sectors × 512 bytes */
    ASSERT(D81_SIZE == 80u * 2u * 10u * 512u);
}

TEST(probe_rejects_d64_sizes) {
    ASSERT(!d81_probe_replica(174848u));
    ASSERT(!d81_probe_replica(196608u));
}

TEST(probe_rejects_d71_size) {
    ASSERT(!d81_probe_replica(349696u));
}

TEST(probe_rejects_off_by_one) {
    ASSERT(!d81_probe_replica(D81_SIZE - 1));
    ASSERT(!d81_probe_replica(D81_SIZE + 1));
}

TEST(probe_rejects_720k_pc_dd) {
    /* 720K PC DD floppy is 737280 bytes — same physical media but
     * different sector layout (9 vs 10 sectors per track).
     * Must not collide with D81 probe. */
    ASSERT(!d81_probe_replica(737280u));
}

TEST(probe_rejects_zero) {
    ASSERT(!d81_probe_replica(0));
}

int main(void) {
    printf("=== D81 plugin probe tests ===\n");
    RUN(probe_exact_size);
    RUN(probe_arithmetic_check);
    RUN(probe_rejects_d64_sizes);
    RUN(probe_rejects_d71_size);
    RUN(probe_rejects_off_by_one);
    RUN(probe_rejects_720k_pc_dd);
    RUN(probe_rejects_zero);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
