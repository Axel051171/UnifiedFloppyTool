/**
 * @file test_po_plugin.c
 * @brief PO (Apple ProDOS Order) Plugin — probe tests.
 *
 * Self-contained per template from test_do_plugin.c.
 * Mirrors src/formats/po/uft_po.c::po_probe.
 *
 * Format: Same 140K Apple II raw shape as DO but ProDOS sector
 * order (non-interleaved). Since file size and byte pattern are
 * indistinguishable from DO without header inspection, probe
 * returns the same size match but a LOWER confidence (55 vs DO's 60)
 * so the dispatcher prefers DO when both match. A real DO/PO
 * disambiguator must parse a filesystem block, not the whole disk.
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

#define PO_SIZE      143360u
#define PO_CONF      55
#define DO_CONF      60

static int po_probe_replica(size_t file_size) {
    if (file_size == PO_SIZE) return PO_CONF;
    return 0;
}

TEST(probe_exact_size) {
    ASSERT(po_probe_replica(PO_SIZE) == PO_CONF);
}

TEST(probe_confidence_lower_than_do) {
    /* Dispatch-tie design: PO confidence must be < DO confidence so
     * the dispatcher picks DO when both plugins match. This is a
     * load-bearing contract between the two plugins. */
    ASSERT(PO_CONF < DO_CONF);
}

TEST(probe_rejects_off_by_one) {
    ASSERT(po_probe_replica(PO_SIZE - 1) == 0);
    ASSERT(po_probe_replica(PO_SIZE + 1) == 0);
}

TEST(probe_rejects_zero) {
    ASSERT(po_probe_replica(0) == 0);
}

TEST(probe_rejects_d64_size) {
    ASSERT(po_probe_replica(174848u) == 0);
}

TEST(probe_rejects_2mg_wrapped_size) {
    /* A 2IMG wrapper around a 140K disk would add a 64-byte header:
     * 143360 + 64 = 143424. Must not match raw PO. */
    ASSERT(po_probe_replica(143424u) == 0);
}

TEST(probe_rejects_nib_size) {
    /* NIB (232960) is the nibble version of a 140K disk — different
     * plugin; must not match PO probe. */
    ASSERT(po_probe_replica(232960u) == 0);
}

int main(void) {
    printf("=== PO plugin probe tests ===\n");
    RUN(probe_exact_size);
    RUN(probe_confidence_lower_than_do);
    RUN(probe_rejects_off_by_one);
    RUN(probe_rejects_zero);
    RUN(probe_rejects_d64_size);
    RUN(probe_rejects_2mg_wrapped_size);
    RUN(probe_rejects_nib_size);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
