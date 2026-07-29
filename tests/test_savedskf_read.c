/**
 * @file test_savedskf_read.c
 * @brief IBM SaveDskF/LoadDskF uncompressed reader (MF-356 rewrite).
 *
 * The prior reader was fabricated against a wrong spec (magic 0x5A4B — matches
 * NO real file; invented geometry offsets; compression mislabelled "LZSS").
 * This rewrites it against the authoritative layout verified from Deark
 * (jsummers/deark modules/fat.c::loaddskf_read_header) + the public-domain
 * dskdcmps LZW decompressor:
 *   +0  BE u16 signature  0xAA58 old / 0xAA59 new / 0xAA5A LZW-compressed
 *   +4  LE u16 bytes/sector      +24 cylinders  +26 heads  +28 sectors/track
 *   +34 sectors-in-image         +38 header size (0=>512; data begins here)
 *   old fmt: sector data at fixed 0x200.
 *
 * Only uncompressed (0xAA58/0xAA59) is decoded; IBM-LZW (0xAA5A) is honestly
 * NOT_IMPLEMENTED pending a ground-truth compressed reference (no unverified
 * codec port — that would risk silent corruption). This test builds synthetic
 * old- and new-format uncompressed images and verifies exact sector recovery,
 * empty-sector reconstruction, LZW deferral, and bad-magic rejection.
 */

#include "uft/formats/pc/uft_savedskf.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* uft_savedskf.h does not expose the error enum; only need "== 0" for success
 * and "!= 0" for failure, so no error-code constants are compared here. */

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define CYLS   20u
#define HEADS  1u
#define SPT    8u
#define SECSZ  512u
#define NSTORE 4u        /* sectors physically present in the image */

static void wbe16(uint8_t *p, uint16_t v) { p[0] = (v >> 8) & 0xFF; p[1] = v & 0xFF; }
static void wle16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }

static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_skf_%d.dsk", dir, rand() % 100000);
}

/* Build a synthetic uncompressed SaveDskF. sig = 0xAA58 (old, data@0x200),
 * 0xAA59 (new, data@hdr_size), or 0xAA5A (LZW). NSTORE sectors are stored, each
 * filled with byte value (0xE0 + sector index) so recovery is checkable. */
static size_t build_skf(const char *path, uint16_t sig) {
    size_t hdr = (sig == UFT_SAVEDSKF_MAGIC_OLD) ? 0x200u : 0x200u; /* both 512 here */
    size_t total = hdr + (size_t)NSTORE * SECSZ;
    uint8_t *file = calloc(1, total);
    if (!file) return 0;

    wbe16(&file[0], sig);
    wle16(&file[UFT_SAVEDSKF_OFF_SECSIZE], SECSZ);
    wle16(&file[UFT_SAVEDSKF_OFF_CYLINDERS], CYLS);
    wle16(&file[UFT_SAVEDSKF_OFF_HEADS], HEADS);
    wle16(&file[UFT_SAVEDSKF_OFF_SPT], SPT);
    wle16(&file[UFT_SAVEDSKF_OFF_NUMSECS], NSTORE);
    wle16(&file[UFT_SAVEDSKF_OFF_HDRSIZE], (uint16_t)hdr);

    for (unsigned s = 0; s < NSTORE; s++)
        memset(&file[hdr + (size_t)s * SECSZ], 0xE0 + s, SECSZ);

    FILE *f = fopen(path, "wb");
    if (!f) { free(file); return 0; }
    size_t w = fwrite(file, 1, total, f);
    fclose(f); free(file);
    return w == total ? total : 0;
}

static void check_uncompressed(uint16_t sig) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_skf(path, sig));

    uft_savedskf_image_t img;
    ASSERT(uft_savedskf_read(path, &img, NULL) == 0);
    ASSERT(img.header.cylinders == CYLS);
    ASSERT(img.header.heads == HEADS);
    ASSERT(img.header.sectors_per_track == SPT);
    ASSERT(img.header.sector_size == SECSZ);
    ASSERT(img.header.compression == UFT_SAVEDSKF_COMP_NONE);
    ASSERT(img.total_sectors == CYLS * HEADS * SPT);
    ASSERT(img.disk_size == (uint32_t)CYLS * HEADS * SPT * SECSZ);

    uint8_t buf[SECSZ];
    /* Stored sectors 1..NSTORE (1-based) carry their marker. */
    for (unsigned s = 0; s < NSTORE; s++) {
        ASSERT(uft_savedskf_read_sector(&img, 0, 0, (uint16_t)(s + 1), buf, sizeof(buf))
               == (int)SECSZ);
        ASSERT(buf[0] == (uint8_t)(0xE0 + s) && buf[SECSZ - 1] == (uint8_t)(0xE0 + s));
    }
    /* A sector past the stored ones is reconstructed empty (calloc zero-fill). */
    ASSERT(uft_savedskf_read_sector(&img, 10, 0, 1, buf, sizeof(buf)) == (int)SECSZ);
    for (unsigned i = 0; i < SECSZ; i++) ASSERT(buf[i] == 0);

    uft_savedskf_image_free(&img);
    remove(path);
}

TEST(new_fmt_uncompressed) { check_uncompressed(UFT_SAVEDSKF_MAGIC_NEW); }
TEST(old_fmt_uncompressed) { check_uncompressed(UFT_SAVEDSKF_MAGIC_OLD); }

TEST(lzw_deferred_not_fabricated) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_skf(path, UFT_SAVEDSKF_MAGIC_LZW));
    uft_savedskf_image_t img;
    /* Compressed data here is not real LZW; the point is that the reader must
     * refuse rather than emit fabricated bytes. */
    ASSERT(uft_savedskf_read(path, &img, NULL) != 0);
    uft_savedskf_image_free(&img);
    remove(path);
}

TEST(probe_matches_be_signature) {
    uint8_t hdr[UFT_SAVEDSKF_HEADER_SIZE];
    memset(hdr, 0, sizeof(hdr));
    wbe16(&hdr[0], UFT_SAVEDSKF_MAGIC_NEW);
    wle16(&hdr[UFT_SAVEDSKF_OFF_SECSIZE], SECSZ);
    wle16(&hdr[UFT_SAVEDSKF_OFF_CYLINDERS], CYLS);
    wle16(&hdr[UFT_SAVEDSKF_OFF_HEADS], HEADS);
    wle16(&hdr[UFT_SAVEDSKF_OFF_SPT], SPT);
    int conf = 0;
    ASSERT(uft_savedskf_probe(hdr, sizeof(hdr), &conf) == true);
    ASSERT(conf > 0);
}

TEST(bad_magic_rejected) {
    uint8_t hdr[UFT_SAVEDSKF_HEADER_SIZE];
    memset(hdr, 0, sizeof(hdr));
    /* Old fabricated magic 0x5A4B (LE) must NOT be accepted now. */
    hdr[0] = 0x4B; hdr[1] = 0x5A;
    wle16(&hdr[UFT_SAVEDSKF_OFF_SECSIZE], SECSZ);
    wle16(&hdr[UFT_SAVEDSKF_OFF_CYLINDERS], CYLS);
    wle16(&hdr[UFT_SAVEDSKF_OFF_HEADS], HEADS);
    wle16(&hdr[UFT_SAVEDSKF_OFF_SPT], SPT);
    int conf = 0;
    ASSERT(uft_savedskf_probe(hdr, sizeof(hdr), &conf) == false);
}

int main(void) {
    printf("=== IBM SaveDskF uncompressed reader (MF-356) ===\n");
    RUN(new_fmt_uncompressed);
    RUN(old_fmt_uncompressed);
    RUN(lzw_deferred_not_fabricated);
    RUN(probe_matches_be_signature);
    RUN(bad_magic_rejected);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
