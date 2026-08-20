/**
 * @file test_convert_scp_adf.c
 * @brief SCP -> ADF: the same Amiga-decoded-as-IBM defect, one layer down
 *        (MF-438).
 *
 * Fourth seam in the ARCH-6 series, and it closes the chain: the corpus SCP is
 * greaseweazle's FLUX capture of xdftool_dd_ofs.adf, the same ADF the HFE in
 * MF-437 came from. So all three representations of one disk — flux, cells,
 * sectors — can be held against each other.
 *
 * `uftc_convert_scp_to_mfm_sectors()` runs a PLL over the flux and then hands
 * the bitstream to `uft_mfm_decode_track()`, described in its own comment as
 * "the canonical MFM IDAM/DAM parser" — IBM System-34. It does that for the
 * ADF target too, where the only concession to Amiga is `sectors = 11`.
 * AmigaDOS has no IDAM and no DAM, so the parser returns nothing, the calloc'd
 * output stays zero, and `result->tracks_converted++` runs anyway. Same shape
 * as MF-437: an 880 KB file of zeros reported as a successful conversion.
 *
 * A second thing in that function says the confusion out loud:
 *
 *     double data_rate = (dst_format == UFT_FORMAT_ADF) ? 500000.0 : 500000.0;
 *
 * A ternary with identical branches — someone meant to separate Amiga from PC
 * and did not.
 *
 * SKIPS with exit 77 when the SCP is absent: the image is 32 MB and lives in
 * gitignored tests/corpus/, so CI cannot run this. It is fully reproducible
 * from the tracked ADF — see tests/corpus_manifest/manifest.json for the exact
 * greaseweazle command.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/flux/uft_flux_decoder.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif
#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR must be defined by the build"
#endif

extern const uft_format_plugin_t uft_format_plugin_scp;

#define SKIP_EXIT 77

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SCP_IMAGE  "gw_amigados.scp"
#define ADF_IMAGE  "xdftool_dd_ofs.adf"
#define ADF_SIZE   901120u
#define ADF_CYLS   80
#define ADF_HEADS  2
#define ADF_SPT    11
#define ADF_SECSZ  512

static const char *scp_path(void)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_RESTRICTED_DIR, SCP_IMAGE);
    return p;
}

static const char *adf_path(void)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_DIR, ADF_IMAGE);
    return p;
}

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *b = (uint8_t *)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    *len = fread(b, 1, (size_t)n, f);
    fclose(f);
    return b;
}

TEST(the_scp_plugin_hands_back_real_flux) {
    /* The seam depends on this: the plugin already parses the SCP header,
     * picks a revolution and converts the sample ticks to nanoseconds, so a
     * caller does not need its own SCP reader — the tree has five of those. */
    uft_disk_t d;
    memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_scp.open(&d, scp_path(), true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_scp.read_track(&d, 0, 0, &t) == UFT_OK);
    ASSERT(t.flux != NULL);
    ASSERT(t.flux_count > 1000);            /* a DD revolution, not a stub */

    /* Amiga DD: a 2 us bit cell, so flux intervals cluster at 4/6/8 us.
     * In 1 ns units that is 4000/6000/8000 — check the bulk lands there
     * rather than trusting the unit claim. */
    size_t in_band = 0;
    uint64_t sum_ns = 0;
    for (size_t i = 0; i < t.flux_count; i++) {
        if (t.flux[i] >= 3000 && t.flux[i] <= 9000) in_band++;
        sum_ns += t.flux[i];
    }
    ASSERT(in_band * 100 / t.flux_count >= 95);

    /* A FULL revolution, not half of one (MF-438).
     *
     * scp_read_revolution_flux() treated the revolution entry's `length` as a
     * byte count and derived length/2 flux values from it. `length` counts
     * 16-bit flux VALUES, so every SCP track came back as exactly half a
     * revolution — silently: sectors still decoded, just five of eleven.
     *
     * The track carries its own duration, so the check needs no external
     * reference: the intervals must add up to the time the disk took to turn
     * once. 300 rpm is 200 ms; 1 % tolerance covers capture jitter. */
    double turn_ns = t.metrics.index_time_ns;
    ASSERT(turn_ns > 190e6 && turn_ns < 210e6);        /* ~300 rpm */
    double ratio = (double)sum_ns / turn_ns;
    if (ratio < 0.99 || ratio > 1.01) {
        printf("\n        flux spans %.2f ms of a %.2f ms revolution (%.0f %%)\n",
               sum_ns / 1e6, turn_ns / 1e6, ratio * 100.0);
    }
    ASSERT(ratio > 0.99 && ratio < 1.01);

    uft_track_release(&t);
    uft_format_plugin_scp.close(&d);
}

TEST(the_ibm_parser_finds_nothing_on_this_disk) {
    /* Why the existing converter produces zeros. Decoded to a bitstream and
     * searched for IBM address marks, an AmigaDOS track yields none — the
     * marks do not exist in the format. Asserted rather than argued. */
    uft_disk_t d;
    memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_scp.open(&d, scp_path(), true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_scp.read_track(&d, 0, 0, &t) == UFT_OK);
    ASSERT(t.flux != NULL);

    /* The plugin hands out ns INTERVALS; flux_raw_data_t wants cumulative
     * TIMES. Two representations, one word — see flux_raw_from_ns_intervals. */
    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals(t.flux, t.flux_count, &raw) == FLUX_OK);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);

    /* The IBM decoder on an Amiga track: no address marks, no sectors. */
    flux_decoded_track_t ibm;
    memset(&ibm, 0, sizeof(ibm));
    flux_decode_mfm(&raw, &ibm, &opts);
    ASSERT(ibm.sector_count == 0);
    flux_decoded_track_free(&ibm);

    /* The Amiga decoder on the same track: all eleven. */
    flux_decoded_track_t ami;
    memset(&ami, 0, sizeof(ami));
    ASSERT(flux_decode_amiga(&raw, &ami, &opts) == FLUX_OK);
    ASSERT(ami.sector_count == ADF_SPT);
    flux_decoded_track_free(&ami);

    flux_raw_free(&raw);
    uft_track_release(&t);
    uft_format_plugin_scp.close(&d);
}

TEST(decoding_the_flux_returns_the_original_adf_byte_for_byte) {
    /* The chain closes. This SCP is greaseweazle's flux capture of
     * xdftool_dd_ofs.adf; MF-437 proved the same for the HFE. Flux, cells and
     * sectors are three representations of one disk, and decoding the deepest
     * one must land back on the shallowest, unchanged.
     *
     * Note what this exercises that the HFE test could not: a real PLL over
     * real captured timings, with the jitter a physical drive produces. */
    size_t want_len = 0;
    uint8_t *want = slurp(adf_path(), &want_len);
    ASSERT(want != NULL);
    ASSERT(want_len == ADF_SIZE);

    uint8_t *got = (uint8_t *)calloc(1, ADF_SIZE);
    ASSERT(got != NULL);

    uft_disk_t d;
    memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_scp.open(&d, scp_path(), true) == UFT_OK);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);

    int sectors_ok = 0, bad_crc = 0;
    for (int cyl = 0; cyl < ADF_CYLS; cyl++) {
        for (int hd = 0; hd < ADF_HEADS; hd++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_scp.read_track(&d, cyl, hd, &t) != UFT_OK ||
                !t.flux || t.flux_count == 0) {
                uft_track_release(&t);
                continue;
            }

            flux_raw_data_t raw;
            if (flux_raw_from_ns_intervals(t.flux, t.flux_count, &raw) != FLUX_OK) {
                uft_track_release(&t);
                continue;
            }

            flux_decoded_track_t dt;
            memset(&dt, 0, sizeof(dt));
            flux_decode_amiga(&raw, &dt, &opts);

            for (int s = 0; s < dt.sector_count; s++) {
                flux_decoded_sector_t *sec = &dt.sectors[s];
                if (!sec->data || sec->data_size != ADF_SECSZ) continue;
                if (sec->sector >= ADF_SPT) continue;
                if (!sec->data_crc_ok) { bad_crc++; continue; }
                size_t off = (((size_t)cyl * ADF_HEADS + (size_t)hd) * ADF_SPT
                              + sec->sector) * ADF_SECSZ;
                if (off + ADF_SECSZ > ADF_SIZE) continue;
                memcpy(got + off, sec->data, ADF_SECSZ);
                sectors_ok++;
            }
            flux_decoded_track_free(&dt);
            flux_raw_free(&raw);
            uft_track_release(&t);
        }
    }
    uft_format_plugin_scp.close(&d);

    ASSERT(bad_crc == 0);
    ASSERT(sectors_ok == ADF_CYLS * ADF_HEADS * ADF_SPT);   /* 1760 */

    size_t differing = 0;
    for (size_t i = 0; i < ADF_SIZE; i++)
        if (want[i] != got[i]) differing++;
    if (differing) {
        printf("\n        %zu of %u bytes differ\n", differing, ADF_SIZE);
    }
    ASSERT(differing == 0);

    free(want); free(got);
}

int main(void)
{
    printf("=== SCP -> ADF: flux decoded as Amiga (MF-438) ===\n");

    FILE *probe = fopen(scp_path(), "rb");
    if (!probe) {
        printf("SKIP: local corpus image absent (%s)\n"
               "      32 MB, gitignored; reproduce with the greaseweazle\n"
               "      command in tests/corpus_manifest/manifest.json\n",
               scp_path());
        return SKIP_EXIT;
    }
    fclose(probe);

    RUN(the_scp_plugin_hands_back_real_flux);
    RUN(the_ibm_parser_finds_nothing_on_this_disk);
    RUN(decoding_the_flux_returns_the_original_adf_byte_for_byte);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
