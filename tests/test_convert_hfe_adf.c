/**
 * @file test_convert_hfe_adf.c
 * @brief HFE -> ADF: the converter decodes Amiga disks as if they were IBM
 *        PC disks, and silently produces nothing (MF-437).
 *
 * The third seam in the ARCH-6 series, and the first one where the existing
 * converter is not merely a duplicate but wrong.
 *
 * `uftc_convert_hfe_to_sectors()` sets `sectors = 11` when the target is ADF
 * and then decodes with IBM System-34 structure: three consecutive 0x4489
 * sync words, an IDAM mark 0xFE carrying C/H/R/N, a DAM mark 0xFB before the
 * data. AmigaDOS has none of that. Its sector is two sync words followed by an
 * odd/even-split info longword, OS label, header and data checksums, then 512
 * bytes — no IDAM, no DAM, and never three syncs in a row.
 *
 * Measured on tests/corpus_free/gw_amigados.hfe, track 0 side 0:
 *
 *     0x4489 syncs        22      = 11 sectors x 2, the AmigaDOS signature
 *     longest sync run     1      the converter requires >= 3
 *     IDAM (0xFE) marks    0
 *     DAM  (0xFB) marks    0
 *     bytes after sync 1   44 89 55 2A AA A5   second sync, then odd/even info
 *
 * So the extraction loop cannot fire even once. The output buffer is calloc'd
 * and stays that way, `result->tracks_converted++` runs unconditionally per
 * head, and `result->success = true` as soon as the file is written: an 880 KB
 * file of zeros, reported as 160 converted tracks. That is not a failed
 * conversion, it is an invented one — the exact thing the first principle
 * forbids.
 *
 * The tree already contains a correct AmigaDOS decoder, `decode_amiga_sector()`
 * in src/flux/uft_flux_decoder.c, with the odd/even scheme handled properly.
 * The converter never used it. MF-437 exposes its bitstream half as
 * `flux_decode_amiga_bits()` — factored, not copied — and reads the track
 * through `uft_format_plugin_hfe`, which already de-interleaves and
 * bit-reverses exactly as the converter did inline.
 *
 * The assertion below is the strongest one available here: gw_amigados.hfe is
 * greaseweazle's MFM encoding of xdftool_dd_ofs.adf (both tracked, T1b), so a
 * correct decode must return that ADF **byte for byte**.
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

extern const uft_format_plugin_t uft_format_plugin_hfe;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define HFE_IMAGE  "gw_amigados.hfe"
#define ADF_IMAGE  "xdftool_dd_ofs.adf"
#define ADF_SIZE   901120u        /* 80 cyl x 2 heads x 11 sectors x 512 */
#define ADF_CYLS   80
#define ADF_HEADS  2
#define ADF_SPT    11
#define ADF_SECSZ  512
#define MFM_SYNC   0x4489

static const char *img(const char *name)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_DIR, name);
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

TEST(the_corpus_hfe_is_amiga_not_ibm) {
    /* The measurement the finding rests on, pinned so it cannot rot. */
    uft_disk_t d;
    memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_hfe.open(&d, img(HFE_IMAGE), true) == UFT_OK);
    ASSERT(d.geometry.cylinders == ADF_CYLS);
    ASSERT(d.geometry.heads == ADF_HEADS);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_hfe.read_track(&d, 0, 0, &t) == UFT_OK);
    ASSERT(t.raw_data != NULL && t.raw_size > 0);

    size_t bits = t.raw_size * 8;
    uint16_t sr = 0;
    int syncs = 0, run = 0, longest = 0, ibm_marks = 0;
    for (size_t b = 0; b < bits; b++) {
        sr = (uint16_t)((sr << 1) | ((t.raw_data[b / 8] >> (7 - (b % 8))) & 1u));
        if (sr == MFM_SYNC) {
            syncs++;
            run++;
            if (run > longest) longest = run;
            /* IBM would put 0xFE or 0xFB here, in the odd bit positions */
            uint8_t mark = 0;
            for (int k = 0; k < 8; k++) {
                size_t mp = b + 1 + (size_t)k * 2 + 1;
                if (mp < bits)
                    mark = (uint8_t)((mark << 1) |
                           ((t.raw_data[mp / 8] >> (7 - (mp % 8))) & 1u));
            }
            if (mark == 0xFE || mark == 0xFB) ibm_marks++;
        } else {
            run = 0;
        }
    }

    ASSERT(syncs == ADF_SPT * 2);   /* two syncs per sector — AmigaDOS */
    ASSERT(longest == 1);           /* never three in a row */
    ASSERT(ibm_marks == 0);         /* no IDAM, no DAM: not an IBM disk */

    uft_track_release(&t);
    uft_format_plugin_hfe.close(&d);
}

TEST(the_ibm_loop_could_not_have_extracted_anything) {
    /* Stated as an assertion rather than left in prose: the converter gates
     * its whole extraction on three consecutive syncs, and this disk never
     * has two. Whatever else it does afterwards is unreachable. */
    uft_disk_t d;
    memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_hfe.open(&d, img(HFE_IMAGE), true) == UFT_OK);

    int tracks_with_three_syncs = 0;
    for (int cyl = 0; cyl < ADF_CYLS; cyl++) {
        for (int hd = 0; hd < ADF_HEADS; hd++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_hfe.read_track(&d, cyl, hd, &t) != UFT_OK ||
                !t.raw_data) { uft_track_release(&t); continue; }
            size_t bits = t.raw_size * 8;
            uint16_t sr = 0;
            int run = 0;
            for (size_t b = 0; b < bits; b++) {
                sr = (uint16_t)((sr << 1) |
                     ((t.raw_data[b / 8] >> (7 - (b % 8))) & 1u));
                if (sr == MFM_SYNC) {
                    if (++run >= 3) break;
                } else {
                    run = 0;
                }
            }
            if (run >= 3) tracks_with_three_syncs++;
            uft_track_release(&t);
        }
    }
    ASSERT(tracks_with_three_syncs == 0);   /* on all 160 track sides */

    uft_format_plugin_hfe.close(&d);
}

TEST(the_amiga_decoder_returns_the_original_adf_byte_for_byte) {
    /* The constructive half. gw_amigados.hfe is greaseweazle's encoding of
     * xdftool_dd_ofs.adf, so a correct decode reproduces that file exactly.
     * Anything less than byte equality here means data was lost or invented. */
    size_t want_len = 0;
    uint8_t *want = slurp(img(ADF_IMAGE), &want_len);
    ASSERT(want != NULL);
    ASSERT(want_len == ADF_SIZE);

    uint8_t *got = (uint8_t *)calloc(1, ADF_SIZE);
    ASSERT(got != NULL);

    uft_disk_t d;
    memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_hfe.open(&d, img(HFE_IMAGE), true) == UFT_OK);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);

    int sectors_ok = 0, bad_data_crc = 0;
    for (int cyl = 0; cyl < ADF_CYLS; cyl++) {
        for (int hd = 0; hd < ADF_HEADS; hd++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_hfe.read_track(&d, cyl, hd, &t) != UFT_OK ||
                !t.raw_data || t.raw_size == 0) {
                uft_track_release(&t);
                continue;
            }

            flux_decoded_track_t dt;
            memset(&dt, 0, sizeof(dt));
            flux_decode_amiga_bits(t.raw_data, t.raw_size * 8, &dt, &opts);

            for (int s = 0; s < dt.sector_count; s++) {
                flux_decoded_sector_t *sec = &dt.sectors[s];
                if (!sec->data || sec->data_size != ADF_SECSZ) continue;
                if (sec->sector >= ADF_SPT) continue;
                if (!sec->data_crc_ok) { bad_data_crc++; continue; }
                size_t off = (((size_t)cyl * ADF_HEADS + (size_t)hd) * ADF_SPT
                              + sec->sector) * ADF_SECSZ;
                if (off + ADF_SECSZ > ADF_SIZE) continue;
                memcpy(got + off, sec->data, ADF_SECSZ);
                sectors_ok++;
            }
            flux_decoded_track_free(&dt);
            uft_track_release(&t);
        }
    }
    uft_format_plugin_hfe.close(&d);

    /* Every sector of an 880K Amiga disk, none with a bad checksum. */
    ASSERT(bad_data_crc == 0);
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
    printf("=== HFE -> ADF: Amiga decoded as Amiga (MF-437) ===\n");
    RUN(the_corpus_hfe_is_amiga_not_ibm);
    RUN(the_ibm_loop_could_not_have_extracted_anything);
    RUN(the_amiga_decoder_returns_the_original_adf_byte_for_byte);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
