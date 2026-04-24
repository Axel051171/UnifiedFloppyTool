/**
 * @file test_d71_plugin.c
 * @brief D71 (Commodore 1571) Plugin — probe tests.
 *
 * Self-contained per template from test_d64_plugin.c.
 * Mirrors src/formats/commodore/d71.c probe logic (size-based match).
 *
 * D71 is a double-sided 1541 — 70 tracks, 1366 sectors, 349696 bytes.
 * Unlike D64, D71 does not have a widely-used error-info variant in
 * the 4-size lookup scheme, so the canonical form is a single exact
 * size match.
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

#define D71_SIZE   349696u

static bool d71_probe_replica(size_t file_size) {
    return file_size == D71_SIZE;
}

TEST(probe_exact_size) {
    ASSERT(d71_probe_replica(D71_SIZE));
}

TEST(probe_rejects_d64_sizes) {
    ASSERT(!d71_probe_replica(174848u));    /* D64 35-track */
    ASSERT(!d71_probe_replica(196608u));    /* D64 40-track */
}

TEST(probe_rejects_d81_size) {
    ASSERT(!d71_probe_replica(819200u));    /* D81 */
}

TEST(probe_rejects_off_by_one) {
    ASSERT(!d71_probe_replica(D71_SIZE - 1));
    ASSERT(!d71_probe_replica(D71_SIZE + 1));
}

TEST(probe_rejects_zero) {
    ASSERT(!d71_probe_replica(0));
}

TEST(probe_rejects_adf_size) {
    ASSERT(!d71_probe_replica(901120u));    /* ADF DD */
}

int main(void) {
    printf("=== D71 plugin probe tests ===\n");
    RUN(probe_exact_size);
    RUN(probe_rejects_d64_sizes);
    RUN(probe_rejects_d81_size);
    RUN(probe_rejects_off_by_one);
    RUN(probe_rejects_zero);
    RUN(probe_rejects_adf_size);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
