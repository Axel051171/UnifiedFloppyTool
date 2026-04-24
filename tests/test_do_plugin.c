/**
 * @file test_do_plugin.c
 * @brief DO (Apple DOS 3.3 Order) Plugin — probe tests.
 *
 * Self-contained per template from test_d64_plugin.c.
 * Mirrors src/formats/do/uft_do.c::do_probe.
 *
 * Format: Headerless 140K Apple II raw disk with DOS 3.3 sector
 * interleave. 35 tracks × 16 sectors × 256 bytes = 143360 bytes.
 * Probe is pure size-match (no content inspection) with confidence
 * 60 (higher than PO's 55 so DO wins the dispatch tie).
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

#define DO_SIZE      143360u    /* 35 × 16 × 256 */
#define DO_CONF      60

static int do_probe_replica(size_t file_size) {
    if (file_size == DO_SIZE) return DO_CONF;
    return 0;
}

TEST(probe_exact_size) {
    ASSERT(do_probe_replica(DO_SIZE) == DO_CONF);
}

TEST(probe_arithmetic_check) {
    ASSERT(DO_SIZE == 35u * 16u * 256u);
}

TEST(probe_rejects_off_by_one) {
    ASSERT(do_probe_replica(DO_SIZE - 1) == 0);
    ASSERT(do_probe_replica(DO_SIZE + 1) == 0);
}

TEST(probe_rejects_zero) {
    ASSERT(do_probe_replica(0) == 0);
}

TEST(probe_rejects_d64_size) {
    /* D64 (174848) is close but different. */
    ASSERT(do_probe_replica(174848u) == 0);
}

TEST(probe_rejects_dsk_size) {
    /* Common CPC DSK sizes — must not collide with DO. */
    ASSERT(do_probe_replica(194816u) == 0);
    ASSERT(do_probe_replica(737280u) == 0);
}

TEST(probe_rejects_double_size) {
    /* 2 × DO_SIZE — might be a Disk II double-sided variant but
     * not standard DO 140K. */
    ASSERT(do_probe_replica(DO_SIZE * 2) == 0);
}

int main(void) {
    printf("=== DO plugin probe tests ===\n");
    RUN(probe_exact_size);
    RUN(probe_arithmetic_check);
    RUN(probe_rejects_off_by_one);
    RUN(probe_rejects_zero);
    RUN(probe_rejects_d64_size);
    RUN(probe_rejects_dsk_size);
    RUN(probe_rejects_double_size);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
