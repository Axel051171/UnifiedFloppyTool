/**
 * @file test_scp_weakbit_multirev.c
 * @brief SCP Klasse-1 forensic guard: multi-revolution weak-bit pass-through.
 *
 * Links the real SCP writer (src/formats/scp/uft_scp_writer.c) and parser
 * (src/flux/uft_scp_parser.c). This is the Phase-4 Klasse-1 work package:
 * "read/write cycle passes flux timings without normalization, multi-rev
 * handling, round-trip of a weak-bit image stays bit-identical in flux
 * timings (documented tolerance band)."
 *
 * A fuzzy/weak-bit region on a real disk reads DIFFERENTLY on each
 * revolution — that per-revolution nondeterminism IS the protection
 * artifact. The forensic requirement (DESIGN_PRINCIPLES "Keine stille
 * Veränderung"): the writer and reader must preserve every revolution
 * verbatim and must NOT collapse the weak window into one deterministic
 * value (e.g. by majority-voting or keeping only revolution 0).
 *
 * Test construction: three revolutions of one track. Head (indices 0-3) and
 * tail (8-11) are identical across all revolutions = stable decoded data.
 * The middle window (4-7) diverges per revolution by well over one SCP tick
 * = the weak region. We write all three, read them back, and assert:
 *   (1) revolution_count == 3            — no revolution dropped
 *   (2) every flux transition round-trips within one 25ns tick — no rounding
 *   (3) the read-back weak windows remain mutually distinct — no collapse
 *
 * TOLERANCE BAND (documented): SCP quantises flux to the 25ns base tick
 * (SCP_TICK_NS). A round-tripped value may therefore differ from the written
 * value by at most one tick (UFT_SCP_BASE_PERIOD_NS). All test flux values
 * are multiples of 25ns so quantisation is exact and the band is the tightest
 * the format physically allows. Anything beyond one tick is a real defect.
 */

#include "uft/formats/uft_scp_writer.h"
#include "uft/flux/uft_scp_parser.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Portable temp path (native Windows has no /tmp) — same fix as CI-1. */
static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_scp_weak_%d.scp", dir, rand() % 100000);
}

#define NREV 3
#define FLEN 12

/* head[0..3] + tail[8..11] identical across revs; weak window[4..7] diverges
   by >> one 25ns tick, exactly how a fuzzy-bit region reads on real hardware. */
static const uint32_t rev_flux[NREV][FLEN] = {
    { 4000,4000,6000,4000,   2000,4000,2000,4000,   8000,4000,6000,4000 },
    { 4000,4000,6000,4000,   4000,2000,6000,2000,   8000,4000,6000,4000 },
    { 4000,4000,6000,4000,   6000,6000,4000,8000,   8000,4000,6000,4000 },
};

static uint32_t rev_duration(int r) {
    uint32_t d = 0;
    for (int i = 0; i < FLEN; i++) d += rev_flux[r][i];
    return d;
}

/* Sum of the weak window (indices 4..7) — a compact fingerprint of the
   region. Distinct sums across revolutions => the region genuinely diverges;
   equal read-back sums for two different written revs => collapse. */
static uint32_t weak_window_sum(const uint32_t *flux) {
    return flux[4] + flux[5] + flux[6] + flux[7];
}

TEST(multirev_weakbit_preserved) {
    scp_writer_t *w = scp_writer_create(0x00, NREV);   /* 3 revolutions */
    ASSERT(w != NULL);
    for (int r = 0; r < NREV; r++) {
        ASSERT(scp_writer_add_track(w, 0, 0, rev_flux[r], FLEN,
                                    rev_duration(r), r) == 0);
    }

    char path[256];
    get_temp_path(path, sizeof(path));
    ASSERT(scp_writer_save(w, path) == 0);
    scp_writer_free(w);

    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));   /* open close-firsts internally */
    ASSERT(uft_scp_open(&ctx, path) == UFT_SCP_OK);

    uft_scp_track_data_t data;
    memset(&data, 0, sizeof(data));
    ASSERT(uft_scp_read_track(&ctx, 0, &data) == UFT_SCP_OK);

    /* (1) no revolution dropped. */
    ASSERT(data.revolution_count == NREV);

    /* (2) every transition of every revolution round-trips within one tick. */
    for (int r = 0; r < NREV; r++) {
        ASSERT(data.revolutions[r].flux_count == FLEN);
        ASSERT(data.revolutions[r].flux_data != NULL);
        for (int i = 0; i < FLEN; i++) {
            uint32_t got = data.revolutions[r].flux_data[i];
            uint32_t want = rev_flux[r][i];
            uint32_t diff = (got > want) ? (got - want) : (want - got);
            ASSERT(diff <= UFT_SCP_BASE_PERIOD_NS);   /* one 25ns tick */
        }
    }

    /* (3) the weak windows stay mutually distinct after round-trip — the
       writer/reader did not majority-vote or keep only one revolution. */
    uint32_t s0 = weak_window_sum(data.revolutions[0].flux_data);
    uint32_t s1 = weak_window_sum(data.revolutions[1].flux_data);
    uint32_t s2 = weak_window_sum(data.revolutions[2].flux_data);
    ASSERT(s0 != s1);
    ASSERT(s1 != s2);
    ASSERT(s0 != s2);

    uft_scp_free_track(&data);
    uft_scp_close(&ctx);
    remove(path);
}

/* Stable regions must be IDENTICAL across revolutions after round-trip —
   proves the divergence in (3) is confined to the weak window and the writer
   is not perturbing stable data. */
TEST(stable_regions_identical_across_revs) {
    scp_writer_t *w = scp_writer_create(0x00, NREV);
    ASSERT(w != NULL);
    for (int r = 0; r < NREV; r++)
        ASSERT(scp_writer_add_track(w, 0, 0, rev_flux[r], FLEN,
                                    rev_duration(r), r) == 0);

    char path[256];
    get_temp_path(path, sizeof(path));
    ASSERT(scp_writer_save(w, path) == 0);
    scp_writer_free(w);

    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(uft_scp_open(&ctx, path) == UFT_SCP_OK);
    uft_scp_track_data_t d;
    memset(&d, 0, sizeof(d));
    ASSERT(uft_scp_read_track(&ctx, 0, &d) == UFT_SCP_OK);
    ASSERT(d.revolution_count == NREV);

    const int stable_idx[] = { 0, 1, 2, 3, 8, 9, 10, 11 };
    for (size_t k = 0; k < sizeof(stable_idx) / sizeof(stable_idx[0]); k++) {
        int i = stable_idx[k];
        uint32_t a = d.revolutions[0].flux_data[i];
        for (int r = 1; r < NREV; r++) {
            uint32_t b = d.revolutions[r].flux_data[i];
            uint32_t diff = (a > b) ? (a - b) : (b - a);
            ASSERT(diff <= UFT_SCP_BASE_PERIOD_NS);
        }
    }

    uft_scp_free_track(&d);
    uft_scp_close(&ctx);
    remove(path);
}

int main(void) {
    printf("=== SCP weak-bit multi-revolution pass-through (Klasse 1) ===\n");
    RUN(multirev_weakbit_preserved);
    RUN(stable_regions_identical_across_revs);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
