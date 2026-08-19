/**
 * @file test_corpus_xfd.c
 * @brief XFD against a cross-tool reference image (MF-426).
 *
 * XFD was T3: no test against real data, no spec verification. The Atari 8-bit
 * FAQ states that XFD is exactly ATR without its 16-byte header
 * (atarimuseum.ctrl-alt-rees.com/archives/atari-8-bit-faq/faq-doc-27.html),
 * which makes the existing cross-tool ATR corpus entry usable for XFD too:
 * strip the header and the result is, by definition, the same disk.
 *
 * That derivation is checked here rather than assumed. The strong assertion is
 * not "the file parses" but "BOTH containers yield byte-identical sector data
 * for the same disk" — which exercises the XFD reader against an independently
 * produced image and cross-checks the ATR reader at the same time.
 *
 * Provenance of the source image (tests/corpus_manifest/manifest.json):
 *   atrcopy 10.1 template dos2sd.atr — 90K Atari DOS 2 single density, empty
 *   VTOC, filesystem structure only, no copyrighted DOS files.
 * The .xfd is that file minus its first 16 bytes; the ATR header itself is
 * verified below, so the derivation is reproducible from the manifest entry.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

extern const uft_format_plugin_t uft_format_plugin_xfd;
extern const uft_format_plugin_t uft_format_plugin_atr;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define XFD_SD_BYTES 92160u          /* 720 sectors x 128 bytes */
#define ATR_HEADER   16u

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

TEST(the_xfd_is_the_atr_without_its_header) {
    /* The derivation itself, checked rather than trusted. The ATR header is
     * decoded field by field so the numbers come from the file, not from the
     * FAQ sentence that prompted this. */
    size_t alen = 0, xlen = 0;
    uint8_t *atr = slurp(img("atrcopy_dos2sd.atr"), &alen);
    uint8_t *xfd = slurp(img("atrcopy_dos2sd.xfd"), &xlen);
    ASSERT(atr && xfd);

    ASSERT(alen == XFD_SD_BYTES + ATR_HEADER);
    ASSERT(xlen == XFD_SD_BYTES);

    /* ATR header: magic 0x0296 LE, size in 16-byte paragraphs, sector size. */
    uint16_t magic = (uint16_t)(atr[0] | (atr[1] << 8));
    uint16_t paras = (uint16_t)(atr[2] | (atr[3] << 8));
    uint16_t secsz = (uint16_t)(atr[4] | (atr[5] << 8));
    ASSERT(magic == 0x0296u);
    ASSERT(secsz == 128u);
    ASSERT((uint32_t)paras * 16u == XFD_SD_BYTES);   /* header agrees with size */

    /* and the payload really is identical */
    ASSERT(memcmp(atr + ATR_HEADER, xfd, XFD_SD_BYTES) == 0);

    free(atr); free(xfd);
}

TEST(xfd_probe_accepts_the_reference_image) {
    size_t len = 0;
    uint8_t *b = slurp(img("atrcopy_dos2sd.xfd"), &len);
    ASSERT(b && len == XFD_SD_BYTES);

    int conf = -1;
    ASSERT(uft_format_plugin_xfd.probe(b, len < 512 ? len : 512, len, &conf));
    /* 40, not 82: the higher score needs an Atari DOS boot sector, and this
     * template is an empty formatted disk whose first sector is all zero.
     * Pinning the lower value keeps the test honest about what is there. */
    ASSERT(conf == 40);
    ASSERT(b[0] == 0x00 && b[3] == 0x00);      /* no boot sector, as expected */

    free(b);
}

TEST(both_containers_yield_identical_sector_data) {
    /* The point of the exercise: one disk, two containers, two independent
     * readers. If the XFD reader mis-handled the missing header — the one
     * structural difference between the formats — this diverges immediately. */
    uft_disk_t dx, da;
    memset(&dx, 0, sizeof(dx)); dx.read_only = true;
    memset(&da, 0, sizeof(da)); da.read_only = true;

    ASSERT(uft_format_plugin_xfd.open(&dx, img("atrcopy_dos2sd.xfd"), true) == UFT_OK);
    ASSERT(uft_format_plugin_atr.open(&da, img("atrcopy_dos2sd.atr"), true) == UFT_OK);

    int compared = 0;
    for (int cyl = 0; cyl < 40; cyl++) {
        uft_track_t tx, ta;
        memset(&tx, 0, sizeof(tx)); memset(&ta, 0, sizeof(ta));
        uft_error_t ex = uft_format_plugin_xfd.read_track(&dx, cyl, 0, &tx);
        uft_error_t ea = uft_format_plugin_atr.read_track(&da, cyl, 0, &ta);
        ASSERT(ex == ea);
        if (ex == UFT_OK) {
            ASSERT(tx.sector_count == ta.sector_count);
            for (size_t s = 0; s < tx.sector_count && s < ta.sector_count; s++) {
                ASSERT(tx.sectors[s].data_len == ta.sectors[s].data_len);
                if (tx.sectors[s].data && ta.sectors[s].data) {
                    ASSERT(memcmp(tx.sectors[s].data, ta.sectors[s].data,
                                  tx.sectors[s].data_len) == 0);
                    compared++;
                }
            }
        }
        for (size_t s = 0; s < tx.sector_count; s++) free(tx.sectors[s].data);
        for (size_t s = 0; s < ta.sector_count; s++) free(ta.sectors[s].data);
        free(tx.sectors); free(ta.sectors);
        free(tx.raw_data); free(ta.raw_data);
    }
    ASSERT(compared > 0);          /* a vacuous pass would be worthless */

    uft_format_plugin_xfd.close(&dx);
    uft_format_plugin_atr.close(&da);
}

int main(void)
{
    printf("=== XFD vs the cross-tool ATR reference (MF-426) ===\n");
    RUN(the_xfd_is_the_atr_without_its_header);
    RUN(xfd_probe_accepts_the_reference_image);
    RUN(both_containers_yield_identical_sector_data);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
