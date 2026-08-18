/**
 * @file test_scp_legacy_adapter.c
 * @brief uft_scp_read() / uft_scp_get_track_flux() over a real SCP (MF-418).
 *
 * Both functions were honest stubs returning -1 unconditionally. Five call
 * sites depended on them and were therefore dead: the four
 * uftc_convert_scp_to_* paths that CLAUDE.md lists as features, and the SCP
 * branch of the protection GUI (KNOWN_ISSUES PROT-11). They are now adapters
 * over the shipped parser in src/flux/uft_scp_parser.c.
 *
 * The reference is tests/corpus/gw_amigados.scp — a cross-tool capture
 * produced by greaseweazle 1.23 from the rights-free xdftool ADF, recorded in
 * tests/corpus_manifest/manifest.json. Local-only, so this test SKIPs when the
 * file is absent rather than passing vacuously.
 *
 * What is checked is agreement with the parser the corpus already verifies,
 * not a second opinion about SCP: for the same track and revolution, the
 * adapter must hand back exactly the values uft_scp_read_track_memory()
 * produced. The unit question that motivated this test is settled the same
 * way — by the shipped parser, which yields nanoseconds.
 */

#include "uft/uft_format_parsers.h"
#include "uft/flux/uft_scp_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR must be defined by the build"
#endif

#define SKIP_EXIT 77

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static uint8_t *g_img = NULL;
static size_t   g_len = 0;

static int load_corpus(void)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/gw_amigados.scp", UFT_CORPUS_RESTRICTED_DIR);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    g_img = (uint8_t *)malloc((size_t)n);
    if (!g_img) { fclose(f); return 0; }
    g_len = fread(g_img, 1, (size_t)n, f);
    fclose(f);
    return g_len == (size_t)n;
}

/** First track index that carries data, or -1. */
static int first_populated_track(const uft_scp_file_t *scp)
{
    for (int t = 0; t < UFT_SCP_MAX_TRACKS; t++)
        if (scp->track_offsets[t] != 0) return t;
    return -1;
}

TEST(read_accepts_the_real_capture) {
    uft_scp_file_t scp;
    memset(&scp, 0, sizeof(scp));

    ASSERT(uft_scp_read(g_img, g_len, &scp) == 0);
    ASSERT(memcmp(scp.header.signature, "SCP", 3) == 0);
    ASSERT(scp.header.revolutions > 0);
    ASSERT(scp.track_offsets != NULL);
    ASSERT(scp.track_count > 0);          /* populated entries, not the table size */
    ASSERT(scp.data != NULL && scp.data_size == g_len);

    uft_scp_free(&scp);
}

TEST(flux_matches_the_shipped_parser_exactly) {
    /* The point of the adapter: no second SCP implementation. For the same
     * track and revolution it must reproduce the parser's values one for one. */
    uft_scp_file_t scp;
    memset(&scp, 0, sizeof(scp));
    ASSERT(uft_scp_read(g_img, g_len, &scp) == 0);

    int track = first_populated_track(&scp);
    ASSERT(track >= 0);

    uft_scp_track_data_t td;
    memset(&td, 0, sizeof(td));
    ASSERT(uft_scp_read_track_memory(scp.data, scp.data_size, track, &td)
           == UFT_SCP_OK);
    ASSERT(td.revolution_count > 0);
    ASSERT(td.revolutions[0].flux_count > 0);

    size_t cap = td.revolutions[0].flux_count;
    double *got = (double *)malloc(cap * sizeof(double));
    ASSERT(got != NULL);

    int n = uft_scp_get_track_flux(&scp, track, 0, got, cap);
    ASSERT(n == (int)cap);

    for (size_t i = 0; i < cap; i++) {
        if (got[i] != (double)td.revolutions[0].flux_data[i]) {
            printf("divergence at %zu: adapter %.1f, parser %u\n",
                   i, got[i], td.revolutions[0].flux_data[i]);
            _fail++;
            break;
        }
    }

    /* Nanoseconds, not raw 25 ns ticks — the distinction the two call sites
     * disagreed on. An Amiga DD revolution is ~200 ms; summing the deltas has
     * to land in that neighbourhood, which raw ticks would miss by ~25x. */
    double total_ns = 0.0;
    for (size_t i = 0; i < cap; i++) total_ns += got[i];
    ASSERT(total_ns > 150e6 && total_ns < 260e6);

    free(got);
    uft_scp_free_track(&td);
    uft_scp_free(&scp);
}

TEST(a_later_revolution_is_reachable) {
    uft_scp_file_t scp;
    memset(&scp, 0, sizeof(scp));
    ASSERT(uft_scp_read(g_img, g_len, &scp) == 0);
    int track = first_populated_track(&scp);
    ASSERT(track >= 0);

    if (scp.header.revolutions < 2) {      /* single-revolution capture */
        uft_scp_free(&scp);
        return;
    }
    double buf[64];
    ASSERT(uft_scp_get_track_flux(&scp, track, 1, buf, 64) == 64);
    ASSERT(buf[0] > 0.0);
    uft_scp_free(&scp);
}

TEST(the_callers_buffer_bounds_the_result) {
    /* Callers pass a fixed array (the GUI uses 65536). The adapter must fill
     * at most that many and say how many, never write past it. */
    uft_scp_file_t scp;
    memset(&scp, 0, sizeof(scp));
    ASSERT(uft_scp_read(g_img, g_len, &scp) == 0);
    int track = first_populated_track(&scp);
    ASSERT(track >= 0);

    double small[17];
    for (size_t i = 0; i < 17; i++) small[i] = -1.0;
    int n = uft_scp_get_track_flux(&scp, track, 0, small, 16);
    ASSERT(n == 16);
    ASSERT(small[16] == -1.0);             /* one past the limit is untouched */
    uft_scp_free(&scp);
}

TEST(rejects_degenerate_arguments) {
    uft_scp_file_t scp;
    memset(&scp, 0, sizeof(scp));
    ASSERT(uft_scp_read(g_img, g_len, &scp) == 0);
    double buf[8];

    ASSERT(uft_scp_get_track_flux(NULL, 0, 0, buf, 8) == -1);
    ASSERT(uft_scp_get_track_flux(&scp, 0, 0, NULL, 8) == -1);
    ASSERT(uft_scp_get_track_flux(&scp, -1, 0, buf, 8) == -1);
    ASSERT(uft_scp_get_track_flux(&scp, UFT_SCP_MAX_TRACKS, 0, buf, 8) == -1);
    ASSERT(uft_scp_get_track_flux(&scp, 0, -1, buf, 8) == -1);
    ASSERT(uft_scp_get_track_flux(&scp, 0, UFT_SCP_MAX_REVOLUTIONS, buf, 8) == -1);
    uft_scp_free(&scp);

    /* Not an SCP, and too short to be one. */
    uft_scp_file_t bad;
    ASSERT(uft_scp_read((const uint8_t *)"NOTSCP", 6, &bad) == -1);
    ASSERT(uft_scp_read(NULL, 1024, &bad) == -1);
}

int main(void)
{
    printf("=== SCP legacy adapters over the real parser (MF-418) ===\n");
    if (!load_corpus()) {
        printf("SKIP: %s/gw_amigados.scp not present (local-only corpus)\n",
               UFT_CORPUS_RESTRICTED_DIR);
        return SKIP_EXIT;
    }
    RUN(read_accepts_the_real_capture);
    RUN(flux_matches_the_shipped_parser_exactly);
    RUN(a_later_revolution_is_reachable);
    RUN(the_callers_buffer_bounds_the_result);
    RUN(rejects_degenerate_arguments);
    free(g_img);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
