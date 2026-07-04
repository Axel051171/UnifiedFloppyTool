/**
 * @file test_scp_writer_roundtrip.c
 * @brief SCP writer — write -> read flux round-trip verification.
 *
 * Links the real writer (src/formats/scp/uft_scp_writer.c) and the real
 * parser (src/flux/uft_scp_parser.c) and proves a written SCP reads back
 * with the same flux: build a track from known flux (multiples of the 25ns
 * SCP tick, so tick-quantisation is exact), save, reopen, and compare the
 * decoded flux transition-for-transition.
 *
 * The SCP writer already existed but was untested and its plugin .write is
 * NULL (FMT-4). This is its safety gate — if the writer emitted a wrong
 * header/track-table/flux encoding, the parser would read back wrong values
 * or fail, and this test would catch it rather than let a corrupt SCP ship.
 */

#include "uft/formats/uft_scp_writer.h"
#include "uft/flux/uft_scp_parser.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Portable temp path (native Windows has no /tmp) — same fix as CI-1. */
static void get_temp_path(char *path, size_t size, const char *ext) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_scp_wr_%d.scp", dir, rand() % 100000);
    (void)ext;
}

/* ── A. flux round-trips write -> read transition-for-transition ────── */

TEST(write_read_flux_roundtrip) {
    /* All multiples of the 25ns SCP tick so quantisation is exact. */
    const uint32_t flux_ns[] = {
        4000, 4000, 6000, 8000, 4000, 4000, 2000, 4000, 6000, 4000,
        4000, 6000, 4000, 8000, 4000, 2000, 4000, 4000, 6000, 4000
    };
    const size_t n = sizeof(flux_ns) / sizeof(flux_ns[0]);
    uint32_t duration = 0;
    for (size_t i = 0; i < n; i++) duration += flux_ns[i];

    scp_writer_t *w = scp_writer_create(0x00, 1);   /* 1 revolution, 25ns res */
    ASSERT(w != NULL);
    ASSERT(scp_writer_add_track(w, 0, 0, flux_ns, n, duration, 0) == 0);

    char path[256];
    get_temp_path(path, sizeof(path), "scp");
    ASSERT(scp_writer_save(w, path) == 0);
    scp_writer_free(w);

    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));   /* uft_scp_open close-first requires zero-init */
    ASSERT(uft_scp_open(&ctx, path) == UFT_SCP_OK);

    uft_scp_track_data_t data;
    memset(&data, 0, sizeof(data));
    ASSERT(uft_scp_read_track(&ctx, 0, &data) == UFT_SCP_OK);
    ASSERT(data.revolution_count >= 1);
    ASSERT(data.revolutions[0].flux_count == n);
    ASSERT(data.revolutions[0].flux_data != NULL);
    for (size_t i = 0; i < n; i++) {
        uint32_t got = data.revolutions[0].flux_data[i];
        uint32_t want = flux_ns[i];
        uint32_t diff = (got > want) ? (got - want) : (want - got);
        ASSERT(diff <= UFT_SCP_BASE_PERIOD_NS);   /* within one 25ns tick */
    }

    uft_scp_free_track(&data);
    uft_scp_close(&ctx);
    remove(path);
}

/* ── B. a second track + side is placed and read independently ─────── */

TEST(second_track_side_roundtrips) {
    const uint32_t fa[] = { 4000, 4000, 4000, 4000 };
    const uint32_t fb[] = { 6000, 6000, 8000 };
    scp_writer_t *w = scp_writer_create(0x00, 1);
    ASSERT(w != NULL);
    ASSERT(scp_writer_add_track(w, 1, 0, fa, 4, 16000, 0) == 0);   /* SCP track 2 */
    ASSERT(scp_writer_add_track(w, 1, 1, fb, 3, 20000, 0) == 0);   /* SCP track 3 */

    char path[256];
    get_temp_path(path, sizeof(path), "scp");
    ASSERT(scp_writer_save(w, path) == 0);
    scp_writer_free(w);

    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));   /* uft_scp_open close-first requires zero-init */
    ASSERT(uft_scp_open(&ctx, path) == UFT_SCP_OK);
    uft_scp_track_data_t d2, d3;
    memset(&d2, 0, sizeof(d2)); memset(&d3, 0, sizeof(d3));
    ASSERT(uft_scp_read_track(&ctx, 2, &d2) == UFT_SCP_OK);
    ASSERT(uft_scp_read_track(&ctx, 3, &d3) == UFT_SCP_OK);
    ASSERT(d2.revolutions[0].flux_count == 4);
    ASSERT(d3.revolutions[0].flux_count == 3);
    uft_scp_free_track(&d2);
    uft_scp_free_track(&d3);
    uft_scp_close(&ctx);
    remove(path);
}

int main(void) {
    printf("=== SCP writer round-trip tests ===\n");
    RUN(write_read_flux_roundtrip);
    RUN(second_track_side_roundtrips);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
