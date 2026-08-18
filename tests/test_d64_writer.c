/**
 * @file test_d64_writer.c
 * @brief C64 GCR operations, tested against the SHIPPED implementation (MF-395).
 *
 * This file used to carry its own 16-entry GCR table, its own gcr_enc() and its
 * own sectors-per-track function, and asserted against those copies. The real
 * GCR code in src/formats/c64/uft_gcr_ops.c was never touched by it.
 *
 * It now exercises the public API: encode/decode round trips and the sync
 * counter — the latter cross-checked against the independent bit-level counter
 * in ufm_c64_metrics_from_gcr() on a REAL VICE-produced G64.
 *
 * That cross-check earned its place immediately: the two counters agree
 * exactly on well-formed disks but diverge on protected ones, because they use
 * different sync definitions (see docs/KNOWN_ISSUES.md FMT-14).
 */

#include "uft/formats/c64/uft_gcr_ops.h"
#include "uft/protection/ufm_c64_metrics.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_g64;

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static const char *corpus_g64(void) {
    static char p[512];
    snprintf(p, sizeof(p), "%s/vice_c1541_35trk.g64", UFT_CORPUS_DIR);
    return p;
}

TEST(gcr_encode_expands_four_bytes_into_five) {
    /* GCR maps each nibble to five bits, so four plain bytes become five. */
    uint8_t plain[4] = { 0x00, 0x11, 0x22, 0x33 };
    uint8_t gcr[16];
    memset(gcr, 0, sizeof(gcr));

    size_t n = gcr_encode(plain, sizeof(plain), gcr);
    ASSERT(n == 5);
}

TEST(gcr_encode_decode_round_trips_exactly) {
    uint8_t plain[64], out[64];
    uint8_t gcr[128];
    for (size_t i = 0; i < sizeof(plain); i++) plain[i] = (uint8_t)(i * 7 + 3);

    size_t enc = gcr_encode(plain, sizeof(plain), gcr);
    ASSERT(enc > 0);

    size_t errors = 0;
    size_t dec = gcr_decode(gcr, enc, out, &errors);
    ASSERT(errors == 0);
    ASSERT(dec >= sizeof(plain));
    ASSERT(memcmp(plain, out, sizeof(plain)) == 0);
}

TEST(gcr_decode_reports_illegal_codes_instead_of_hiding_them) {
    /* A GCR stream of zero bits contains no legal 5-bit code at all. The
     * decoder must say so rather than return silence. */
    uint8_t gcr[20];
    memset(gcr, 0x00, sizeof(gcr));
    uint8_t out[32];
    size_t errors = 0;

    gcr_decode(gcr, sizeof(gcr), out, &errors);
    ASSERT(errors > 0);
}

TEST(killer_and_empty_track_detection_on_synthetic_extremes) {
    uint8_t all_ff[512], all_00[512];
    memset(all_ff, 0xFF, sizeof(all_ff));
    memset(all_00, 0x00, sizeof(all_00));

    ASSERT(gcr_is_killer_track(all_ff, sizeof(all_ff)));
    ASSERT(!gcr_is_killer_track(all_00, sizeof(all_00)));
    ASSERT(gcr_is_empty_track(all_00, sizeof(all_00)));
}

TEST(sync_counters_agree_on_a_real_formatted_disk) {
    /* Two independent implementations on real GCR: the byte-aligned counter in
     * uft_gcr_ops.c and the bit-level one in ufm_c64_metrics.c. On a cleanly
     * formatted disk they must agree track for track — pinned total 1366 over
     * 35 tracks (42 syncs on each of the 17 zone-3 tracks, and so on). */
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_g64.open(&disk, corpus_g64(), true) == UFT_OK);

    int total_byte = 0, total_bit = 0, tracks = 0;
    for (int cyl = 0; cyl < 35; cyl++) {
        uft_track_t t; memset(&t, 0, sizeof(t));
        ASSERT(uft_format_plugin_g64.read_track(&disk, cyl, 0, &t) == UFT_OK);
        ASSERT(t.raw_data != NULL);

        int byte_syncs = gcr_count_syncs_bytealigned(t.raw_data, t.raw_size);
        ufm_c64_track_metrics_t m;
        ASSERT(ufm_c64_metrics_from_gcr(t.raw_data, t.raw_size, cyl * 2,
                                        UFM_C64_SPEED_ZONE_AUTO, &m));
        ASSERT(byte_syncs == (int)m.sync_count);   /* per track, not just in sum */

        total_byte += byte_syncs;
        total_bit += (int)m.sync_count;
        tracks++;

        free(t.raw_data);
        for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
        free(t.sectors);
    }
    uft_format_plugin_g64.close(&disk);

    ASSERT(tracks == 35);
    ASSERT(total_byte == 1366);
    ASSERT(total_bit == 1366);
}

TEST(the_two_sync_definitions_differ_by_construction) {
    /* Pins WHY they can diverge, without needing a protected image: production
     * accepts a byte-aligned 0xFF followed by a set MSB, i.e. nine one-bits;
     * the metric extractor requires ten consecutive one-bits at any alignment,
     * which is the 1541 hardware condition. A single 0xFF plus one set bit is
     * therefore a sync to one and not to the other. See FMT-14. */
    uint8_t buf[8];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0xFF;          /* eight ones ... */
    buf[1] = 0x80;          /* ... plus one more, then zeros: nine in total */

    ASSERT(gcr_count_syncs_bytealigned(buf, sizeof(buf)) == 1);

    ufm_c64_track_metrics_t m;
    ASSERT(ufm_c64_metrics_from_gcr(buf, sizeof(buf), 0, 3, &m));
    ASSERT(m.sync_count == 0);          /* nine < ten */
    ASSERT(m.max_sync_run_bits == 9);
}

int main(void) {
    printf("=== C64 GCR operations, real implementation (MF-395) ===\n");
    RUN(gcr_encode_expands_four_bytes_into_five);
    RUN(gcr_encode_decode_round_trips_exactly);
    RUN(gcr_decode_reports_illegal_codes_instead_of_hiding_them);
    RUN(killer_and_empty_track_detection_on_synthetic_extremes);
    RUN(sync_counters_agree_on_a_real_formatted_disk);
    RUN(the_two_sync_definitions_differ_by_construction);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
