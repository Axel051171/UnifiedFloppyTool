/**
 * @file test_sector_id_on_disk.c
 * @brief uft_sector_t.id.sector is the number ON the disk, not a loop index
 *        (ARCH-20, MF-465).
 *
 * The shared helper `uft_format_add_sector()` sets `id.sector = sector_num + 1`
 * for every plugin that uses it — 71 of them. For IBM PC formats (sectors
 * 1..N) that is right. For Apple (0..15), Amiga (0..10) and Commodore
 * (0..N-1) it invents a number that is not on the medium.
 *
 * The proof needs no synthetic assumption, because the corpus contains **the
 * same disk twice**, written by the same canonical third-party tool:
 *
 *     tests/corpus_free/vice_c1541_35trk.d64
 *     tests/corpus_free/vice_c1541_35trk.g64
 *
 * The G64 is a GCR bitstream, so its sector numbers are the bytes VICE wrote
 * into the 1541 header blocks — ground truth — and our G64 reader takes them
 * verbatim (`uft_g64.c:621`). The D64 is the same disk as a sector dump. If
 * the two readers disagree about which numbers are on that disk, one of them
 * is making them up. Before MF-465 the D64 side said 1..20 where the G64 side
 * said 0..19.
 *
 * The consequence was not academic: `g64_write_track()` encodes `id.sector`
 * straight into the GCR header (`uft_g64.c:786`), so the generic
 * plugin-to-plugin conversion path (`src/core/uft_disk_convert.c`) would have
 * written sector numbers 1..21 onto a disk that a 1541 can only read as
 * 0..20. Only the dedicated `d64_to_g64()` compensated, with a `- 1` and a
 * comment saying "ID is 1-based".
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_format_common.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_g64;
extern const uft_format_plugin_t uft_format_plugin_adf;
extern const uft_format_plugin_t uft_format_plugin_do;
extern const uft_format_plugin_t uft_format_plugin_img;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static const char *img(const char *name)
{
    static char buf[1024];
    snprintf(buf, sizeof(buf), "%s/%s", UFT_CORPUS_DIR, name);
    return buf;
}

static void get_temp_path(char *path, size_t size, const char *tag, const char *ext)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_secid_%s_%d.%s", dir, tag, rand() % 100000, ext);
}

static int cmp_u8(const void *a, const void *b)
{
    return (int)*(const uint8_t *)a - (int)*(const uint8_t *)b;
}

/** Sorted list of the sector numbers a track reports. */
static size_t ids_of(const uft_track_t *tr, uint8_t *out, size_t max)
{
    size_t n = 0;
    for (size_t i = 0; i < tr->sector_count && n < max; i++)
        out[n++] = tr->sectors[i].id.sector;
    qsort(out, n, sizeof(out[0]), cmp_u8);
    return n;
}

TEST(d64_and_g64_agree_on_the_same_disk)
{
    uft_disk_t d64, g64;
    memset(&d64, 0, sizeof(d64));
    memset(&g64, 0, sizeof(g64));

    ASSERT(uft_format_plugin_d64.open(&d64, img("vice_c1541_35trk.d64"), true) == UFT_OK);
    ASSERT(uft_format_plugin_g64.open(&g64, img("vice_c1541_35trk.g64"), true) == UFT_OK);

    /* Track 1 (zone 0, 21 sectors), track 18 (the BAM/directory track) and
     * track 35 (the innermost zone, 17 sectors) — one per interesting case. */
    const int cyls[] = { 0, 17, 34 };
    int compared = 0;

    for (size_t k = 0; k < sizeof(cyls) / sizeof(cyls[0]); k++) {
        uft_track_t ta, tb;
        memset(&ta, 0, sizeof(ta));
        memset(&tb, 0, sizeof(tb));

        if (uft_format_plugin_d64.read_track(&d64, cyls[k], 0, &ta) != UFT_OK) {
            uft_track_release(&ta);
            continue;
        }
        if (uft_format_plugin_g64.read_track(&g64, cyls[k], 0, &tb) != UFT_OK ||
            tb.sector_count == 0) {
            uft_track_release(&ta);
            uft_track_release(&tb);
            continue;   /* GCR decode did not recover this track — not our subject */
        }

        uint8_t ia[32], ib[32];
        size_t na = ids_of(&ta, ia, sizeof(ia));
        size_t nb = ids_of(&tb, ib, sizeof(ib));

        if (na != nb || memcmp(ia, ib, na) != 0) {
            printf("FAIL track %d: D64 says", cyls[k] + 1);
            for (size_t i = 0; i < na; i++) printf(" %u", ia[i]);
            printf(", G64 says");
            for (size_t i = 0; i < nb; i++) printf(" %u", ib[i]);
            printf("\n");
            _fail++;
            uft_track_release(&ta);
            uft_track_release(&tb);
            return;
        }
        /* and the numbers a 1541 writes start at zero */
        ASSERT(na > 0 && ia[0] == 0);
        compared++;

        uft_track_release(&ta);
        uft_track_release(&tb);
    }

    /* If nothing could be compared the test proves nothing — say so. */
    ASSERT(compared > 0);

    uft_format_plugin_d64.close(&d64);
    uft_format_plugin_g64.close(&g64);
}

TEST(sector_zero_can_be_found_by_its_number)
{
    /* The concrete consequence named in ARCH-20: a lookup by the number that
     * is on the disk. Track 18 sector 0 is the BAM — the single most-looked-up
     * sector on a Commodore disk. */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_d64.open(&disk, img("vice_c1541_35trk.d64"), true) == UFT_OK);

    uft_track_t bam;
    memset(&bam, 0, sizeof(bam));
    ASSERT(uft_format_plugin_d64.read_track(&disk, 17, 0, &bam) == UFT_OK);

    const uft_sector_t *s0 = NULL;
    for (size_t i = 0; i < bam.sector_count; i++)
        if (bam.sectors[i].id.sector == 0) { s0 = &bam.sectors[i]; break; }

    ASSERT(s0 != NULL);
    ASSERT(s0->data != NULL);
    /* c1541 -format "uftcorpus,42" → the disk ID sits at 0xA2 of the BAM */
    ASSERT(s0->data[0xA2] == '4' && s0->data[0xA3] == '2');

    uft_track_release(&bam);
    uft_format_plugin_d64.close(&disk);
}

TEST(amiga_sectors_start_at_zero)
{
    /* AmigaDOS DD: 80 cylinders x 2 heads x 11 sectors x 512 = 901,120 */
    char path[512];
    get_temp_path(path, sizeof(path), "adf", "adf");
    FILE *f = fopen(path, "wb");
    ASSERT(f != NULL);
    uint8_t sector[512];
    memset(sector, 0, sizeof(sector));
    for (int i = 0; i < 80 * 2 * 11; i++) fwrite(sector, 1, sizeof(sector), f);
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_adf.open(&disk, path, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    ASSERT(uft_format_plugin_adf.read_track(&disk, 40, 1, &tr) == UFT_OK);
    ASSERT(tr.sector_count == 11);
    for (size_t i = 0; i < tr.sector_count; i++)
        ASSERT(tr.sectors[i].id.sector == (uint8_t)i);   /* 0..10, not 1..11 */

    uft_track_release(&tr);
    uft_format_plugin_adf.close(&disk);
    remove(path);
}

TEST(apple_sectors_start_at_zero)
{
    /* Apple II DOS 3.3: 35 tracks x 16 sectors x 256 = 143,360 */
    char path[512];
    get_temp_path(path, sizeof(path), "do", "do");
    FILE *f = fopen(path, "wb");
    ASSERT(f != NULL);
    uint8_t sector[256];
    memset(sector, 0, sizeof(sector));
    for (int i = 0; i < 35 * 16; i++) fwrite(sector, 1, sizeof(sector), f);
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_do.open(&disk, path, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    ASSERT(uft_format_plugin_do.read_track(&disk, 17, 0, &tr) == UFT_OK);
    ASSERT(tr.sector_count == 16);
    for (size_t i = 0; i < tr.sector_count; i++)
        ASSERT(tr.sectors[i].id.sector == (uint8_t)i);   /* 0..15, not 1..16 */

    uft_track_release(&tr);
    uft_format_plugin_do.close(&disk);
    remove(path);
}

TEST(img_write_keeps_a_zero_based_source_complete)
{
    /* The other half of ARCH-20. img_write_track() looked its sectors up as
     * 1..N and padded every miss with zeros, silently. Handed a track that is
     * numbered the way an Amiga or a 1541 numbers one, it dropped sector 0 and
     * appended a zero-filled sector nobody had read. It now writes the track's
     * sectors in ascending on-disk order (MF-465). */
    char path[512];
    get_temp_path(path, sizeof(path), "img", "img");

    enum { IMG_SPT = 18, IMG_SS = 512 };   /* 1.44 MB — a geometry img_open knows */

    /* An empty target of the right size, so open() finds a geometry. */
    FILE *f = fopen(path, "wb");
    ASSERT(f != NULL);
    uint8_t blank[IMG_SS];
    memset(blank, 0, sizeof(blank));
    for (int i = 0; i < 80 * 2 * IMG_SPT; i++) fwrite(blank, 1, sizeof(blank), f);
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_img.open(&disk, path, false) == UFT_OK);
    ASSERT(disk.geometry.sectors == IMG_SPT);
    disk.read_only = false;

    /* A track numbered 0..10, each sector carrying its own number. */
    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    uft_track_init(&tr, 3, 1);
    uint8_t payload[IMG_SS];
    for (int s = 0; s < IMG_SPT; s++) {
        memset(payload, (uint8_t)(0xA0 + s), sizeof(payload));
        ASSERT(uft_format_add_sector_with_id(&tr, (uint8_t)s, payload, IMG_SS, 3, 1) == UFT_OK);
    }
    ASSERT(uft_format_plugin_img.write_track(&disk, 3, 1, &tr) == UFT_OK);
    uft_track_release(&tr);
    uft_format_plugin_img.close(&disk);

    /* Every one of the eleven sectors must be on disk, in order. */
    f = fopen(path, "rb");
    ASSERT(f != NULL);
    long off = ((long)3 * 2 + 1) * IMG_SPT * IMG_SS;
    ASSERT(fseek(f, off, SEEK_SET) == 0);
    for (int s = 0; s < IMG_SPT; s++) {
        uint8_t got[IMG_SS];
        ASSERT(fread(got, 1, IMG_SS, f) == IMG_SS);
        if (got[0] != (uint8_t)(0xA0 + s)) {
            printf("FAIL: slot %d holds 0x%02X, expected 0x%02X\n",
                   s, got[0], 0xA0 + s);
            _fail++;
            fclose(f);
            remove(path);
            return;
        }
    }
    fclose(f);
    remove(path);
}

int main(void)
{
    printf("=== Sektor-IDs sind die Nummern auf der Diskette (ARCH-20, MF-465) ===\n");
    RUN(d64_and_g64_agree_on_the_same_disk);
    RUN(sector_zero_can_be_found_by_its_number);
    RUN(amiga_sectors_start_at_zero);
    RUN(apple_sectors_start_at_zero);
    RUN(img_write_keeps_a_zero_based_source_complete);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
