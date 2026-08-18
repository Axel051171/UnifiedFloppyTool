/**
 * @file test_plugin_probe_real.c
 * @brief Probes the REAL plugins — wave 1 (MF-385).
 *
 * Replaces the "replica" pattern. Those tests copied the probe logic into the
 * test file and asserted against the copy, so production could drift away
 * without a single test turning red. The audit found 32 of them; this file
 * starts converting them into tests that call the shipped code.
 *
 * The drift was not hypothetical: test_d64_plugin.c knew four accepted D64
 * sizes, while d64_plugin_probe() accepts eight (the 41/42-track VICE/Schepers
 * variants were added to production and never reached the replica).
 *
 * Every expectation below was read out of the plugin source, not assumed:
 *   d64  8 sizes incl. 200960/201745/205312/206114 (uft_d64_plugin.c:38)
 *   d71  349696 / 351062                            (uft_d71.c:13)
 *   d81  819200 / 822400                            (uft_d81.c:12)
 *   g64  signature "GCR-1541", >= 12 bytes          (uft_g64.c:39,45)
 *   hfe  "HXCPICFE" or "HXCHFEV3"                   (uft_hfe.c:39,40)
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_d71;
extern const uft_format_plugin_t uft_format_plugin_d81;
extern const uft_format_plugin_t uft_format_plugin_g64;
extern const uft_format_plugin_t uft_format_plugin_hfe;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/** Call a real probe with a header buffer and a claimed file size. */
static bool probe_sized(const uft_format_plugin_t *p, const uint8_t *hdr,
                        size_t hdr_len, size_t file_size, int *conf)
{
    int c = -1;
    bool ok = p->probe(hdr, hdr_len, file_size, &c);
    if (conf) *conf = c;
    return ok;
}

TEST(d64_accepts_all_eight_production_sizes) {
    /* The replica knew four; production accepts eight. */
    static const size_t accepted[] = {
        174848, 175531, 196608, 197376,     /* 35 / 40 tracks, +/- error map */
        200960, 201745, 205312, 206114      /* 41 / 42 tracks (VICE/Schepers) */
    };
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));

    for (size_t i = 0; i < sizeof(accepted) / sizeof(accepted[0]); i++) {
        int conf = -1;
        ASSERT(probe_sized(&uft_format_plugin_d64, hdr, sizeof(hdr),
                           accepted[i], &conf));
        ASSERT(conf > 0);
    }
}

TEST(d64_rejects_neighbouring_sizes) {
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));
    int conf = -1;
    ASSERT(!probe_sized(&uft_format_plugin_d64, hdr, sizeof(hdr), 174847, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_d64, hdr, sizeof(hdr), 174849, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_d64, hdr, sizeof(hdr), 0, &conf));
}

TEST(d64_confidence_rises_with_a_real_bam_link) {
    /* Production raises confidence from 75 to 92 when the BAM at 0x16500
     * carries the 18/1 directory link. Exercising that needs a full image. */
    size_t sz = 174848;
    uint8_t *img = calloc(1, sz);
    ASSERT(img != NULL);

    int plain = -1;
    ASSERT(probe_sized(&uft_format_plugin_d64, img, sz, sz, &plain));

    img[0x16500] = 18;
    img[0x16501] = 1;
    int with_bam = -1;
    ASSERT(probe_sized(&uft_format_plugin_d64, img, sz, sz, &with_bam));
    ASSERT(with_bam > plain);

    free(img);
}

TEST(d71_accepts_both_variants_and_rejects_others) {
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));
    int conf = -1;
    ASSERT(probe_sized(&uft_format_plugin_d71, hdr, sizeof(hdr), 349696, &conf));
    ASSERT(probe_sized(&uft_format_plugin_d71, hdr, sizeof(hdr), 351062, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_d71, hdr, sizeof(hdr), 349695, &conf));
    /* a D64 must not be claimed by the D71 plugin */
    ASSERT(!probe_sized(&uft_format_plugin_d71, hdr, sizeof(hdr), 174848, &conf));
}

TEST(d81_accepts_both_variants_and_rejects_others) {
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));
    int conf = -1;
    ASSERT(probe_sized(&uft_format_plugin_d81, hdr, sizeof(hdr), 819200, &conf));
    ASSERT(probe_sized(&uft_format_plugin_d81, hdr, sizeof(hdr), 822400, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_d81, hdr, sizeof(hdr), 819199, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_d81, hdr, sizeof(hdr), 349696, &conf));
}

TEST(g64_requires_its_signature) {
    uint8_t hdr[32];
    memset(hdr, 0, sizeof(hdr));
    int conf = -1;

    ASSERT(!probe_sized(&uft_format_plugin_g64, hdr, sizeof(hdr), sizeof(hdr), &conf));

    memcpy(hdr, "GCR-1541", 8);
    ASSERT(probe_sized(&uft_format_plugin_g64, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf > 0);

    /* one byte off is not a G64 */
    hdr[7] = '0';
    ASSERT(!probe_sized(&uft_format_plugin_g64, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* too short to hold a header at all */
    memcpy(hdr, "GCR-1541", 8);
    ASSERT(!probe_sized(&uft_format_plugin_g64, hdr, 8, 8, &conf));
}

TEST(hfe_accepts_v1_and_v3_signatures) {
    /* hfe_header_t is padded to 512 bytes and the probe refuses anything
     * shorter — found by this test on its first run, when a 64-byte buffer
     * was rejected for BOTH signatures. */
    uint8_t hdr[512];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "HXCPICFE", 8);
    ASSERT(probe_sized(&uft_format_plugin_hfe, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "HXCHFEV3", 8);
    ASSERT(probe_sized(&uft_format_plugin_hfe, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "HXCPICFF", 8);
    ASSERT(!probe_sized(&uft_format_plugin_hfe, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* a truncated header is not enough, even with a valid signature */
    memcpy(hdr, "HXCPICFE", 8);
    ASSERT(!probe_sized(&uft_format_plugin_hfe, hdr, 64, 64, &conf));
}

TEST(plugins_do_not_claim_each_others_headers) {
    /* Cross-check: a G64 header must not be accepted by HFE and vice versa.
     * A replica test cannot do this at all — it only ever sees its own copy. */
    uint8_t g64_hdr[512], hfe_hdr[512];
    memset(g64_hdr, 0, sizeof(g64_hdr));
    memset(hfe_hdr, 0, sizeof(hfe_hdr));
    memcpy(g64_hdr, "GCR-1541", 8);
    memcpy(hfe_hdr, "HXCPICFE", 8);
    int conf = -1;

    ASSERT(!probe_sized(&uft_format_plugin_hfe, g64_hdr, sizeof(g64_hdr),
                        sizeof(g64_hdr), &conf));
    ASSERT(!probe_sized(&uft_format_plugin_g64, hfe_hdr, sizeof(hfe_hdr),
                        sizeof(hfe_hdr), &conf));
}

int main(void) {
    printf("=== Real plugin probes, wave 1 (MF-385) ===\n");
    RUN(d64_accepts_all_eight_production_sizes);
    RUN(d64_rejects_neighbouring_sizes);
    RUN(d64_confidence_rises_with_a_real_bam_link);
    RUN(d71_accepts_both_variants_and_rejects_others);
    RUN(d81_accepts_both_variants_and_rejects_others);
    RUN(g64_requires_its_signature);
    RUN(hfe_accepts_v1_and_v3_signatures);
    RUN(plugins_do_not_claim_each_others_headers);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
