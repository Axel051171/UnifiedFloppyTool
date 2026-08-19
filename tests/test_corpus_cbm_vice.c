/**
 * @file test_corpus_cbm_vice.c
 * @brief D67 / D80 / D82 / G71 against c1541-produced reference images (MF-427).
 *
 * Four formats that had no test against real data at all. The images come from
 * VICE's c1541, the reference implementation for Commodore disk containers, so
 * the geometry they encode is produced outside UFT's own assumptions — which is
 * the whole point of the T1b tier.
 *
 * Provenance (tests/corpus_manifest/manifest.json):
 *   c1541 -format "uftcorpus,42" <type> <img> -write marker.txt "marker"
 *   VICE 3.10 (SDLVICE-3.10-win64-r46215). marker.txt is original UFT text.
 *
 * What the reference data already corrected: d80_probe/d82_probe looked for a
 * BAM link at file offset 0x33000 and expected it to point at 39/1. On a real
 * 8050 image 0x33000 is empty space mid-track-29, and the header block — track
 * 39 sector 0, i.e. offset 0x44E00 — links to 38/0. The old constants were the
 * D64 convention copied onto a format with a different zone table. The tests
 * below pin the corrected values against the actual bytes.
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

extern const uft_format_plugin_t uft_format_plugin_d67;
extern const uft_format_plugin_t uft_format_plugin_d80;
extern const uft_format_plugin_t uft_format_plugin_d82;
extern const uft_format_plugin_t uft_format_plugin_g71;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* The window uft_probe_file_format() actually hands a probe. Kept as a named
 * constant so the "only size discriminates in production" assertions below say
 * what they mean. */
#define REGISTRY_PROBE_WINDOW 4096u

#define D67_SIZE 176640u
#define D80_SIZE 533248u
#define D82_SIZE 1066496u
#define CBM_HEADER_OFF 0x44E00u   /* D80/D82 track 39 sector 0 */

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

static void free_track(uft_track_t *t)
{
    for (size_t s = 0; s < t->sector_count; s++) free(t->sectors[s].data);
    free(t->sectors);
    free(t->raw_data);
}

/* ---------------------------------------------------------------- D67 ---- */

TEST(d67_zone_table_matches_the_c1541_image) {
    /* The 2040/4040 zone table is what separates D67 from D64: tracks 18-24
     * carry 20 sectors where a 1541 carries 19. If the plugin had inherited
     * the D64 table, the file would be 1792 bytes short and every track from
     * 18 on would be read at the wrong offset. */
    uft_disk_t d; memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_d67.open(&d, img("vice_c1541_2040.d67"), true) == UFT_OK);
    ASSERT(d.geometry.cylinders == 35);
    ASSERT(d.geometry.heads == 1);
    ASSERT(d.geometry.total_sectors == 690);
    ASSERT(d.geometry.sector_size == 256);

    int total = 0;
    for (int cyl = 0; cyl < 35; cyl++) {
        uft_track_t t; memset(&t, 0, sizeof(t));
        ASSERT(uft_format_plugin_d67.read_track(&d, cyl, 0, &t) == UFT_OK);
        int track = cyl + 1;
        size_t want = track <= 17 ? 21u : track <= 24 ? 20u : track <= 30 ? 18u : 17u;
        ASSERT(t.sector_count == want);
        total += (int)t.sector_count;
        free_track(&t);
    }
    ASSERT(total == 690);
    uft_format_plugin_d67.close(&d);
}

TEST(d67_bam_and_disk_name_are_where_the_reader_looks) {
    /* Track 18 sector 0 read through the plugin, not by seeking into the file:
     * this is what proves the offset arithmetic, since a wrong zone table
     * would land somewhere else entirely. */
    uft_disk_t d; memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_d67.open(&d, img("vice_c1541_2040.d67"), true) == UFT_OK);

    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d67.read_track(&d, 17, 0, &t) == UFT_OK);  /* track 18 */
    ASSERT(t.sector_count == 20);
    const uint8_t *bam = t.sectors[0].data;
    ASSERT(bam != NULL && t.sectors[0].data_len == 256);
    ASSERT(bam[0] == 18 && bam[1] == 1);              /* link to the directory */
    ASSERT(memcmp(bam + 0x90, "UFTCORPUS", 9) == 0);  /* c1541 -format name */

    free_track(&t);
    uft_format_plugin_d67.close(&d);
}

TEST(d67_probe_scores_88_only_with_the_whole_file) {
    size_t len = 0;
    uint8_t *b = slurp(img("vice_c1541_2040.d67"), &len);
    ASSERT(b && len == D67_SIZE);

    int conf = -1;
    ASSERT(uft_format_plugin_d67.probe(b, len, len, &conf));
    ASSERT(conf == 88);                    /* BAM seen */

    /* And what the registry really gets: the BAM sits at 0x16500, far past the
     * 4 KiB window, so in production D67 is recognised by size alone. Asserting
     * this keeps the number honest instead of advertising 88. */
    conf = -1;
    ASSERT(uft_format_plugin_d67.probe(b, REGISTRY_PROBE_WINDOW, len, &conf));
    ASSERT(conf == 75);

    conf = -1;
    ASSERT(!uft_format_plugin_d67.probe(b, REGISTRY_PROBE_WINDOW, len - 1, &conf));
    free(b);
}

/* ------------------------------------------------------------ D80/D82 ---- */

TEST(d80_header_block_sits_at_track_39_sector_0) {
    size_t len = 0;
    uint8_t *b = slurp(img("vice_c1541_8050.d80"), &len);
    ASSERT(b && len == D80_SIZE);

    /* 38 tracks x 29 sectors x 256 = 0x44E00. The bytes there, from c1541:
     * link 38/0 (first BAM block), then 'C' = CBM DOS 2.7. */
    ASSERT(b[CBM_HEADER_OFF] == 38);
    ASSERT(b[CBM_HEADER_OFF + 1] == 0);
    ASSERT(b[CBM_HEADER_OFF + 2] == 'C');
    ASSERT(memcmp(b + CBM_HEADER_OFF + 6, "UFTCORPUS", 9) == 0);

    /* the constant the probe used before: empty space */
    ASSERT(b[0x33000] == 0 && b[0x33001] == 0);

    int conf = -1;
    ASSERT(uft_format_plugin_d80.probe(b, len, len, &conf));
    ASSERT(conf == 90);
    conf = -1;
    ASSERT(uft_format_plugin_d80.probe(b, REGISTRY_PROBE_WINDOW, len, &conf));
    ASSERT(conf == 75);                    /* registry window: size only */
    free(b);
}

TEST(d82_is_the_double_sided_8050) {
    size_t len = 0;
    uint8_t *b = slurp(img("vice_c1541_8250.d82"), &len);
    ASSERT(b && len == D82_SIZE);
    ASSERT(len == 2u * D80_SIZE);          /* 2 x 2083 sectors x 256 */

    ASSERT(b[CBM_HEADER_OFF] == 38 && b[CBM_HEADER_OFF + 2] == 'C');

    /* What actually distinguishes 8250 from 8050 on the medium: the second
     * side needs two more BAM blocks, which CBM DOS 2.7 places at 38/6 and
     * 38/9 — non-empty here, absent on the 8050 image. */
    const size_t t38s0 = 37u * 29u * 256u;
    int extra = 0;
    for (int s = 6; s <= 9; s += 3) {
        const uint8_t *blk = b + t38s0 + (size_t)s * 256u;
        for (int i = 0; i < 256; i++) if (blk[i]) { extra++; break; }
    }
    ASSERT(extra == 2);

    int conf = -1;
    ASSERT(uft_format_plugin_d82.probe(b, len, len, &conf));
    ASSERT(conf == 88);
    /* the size gate is what separates the two formats, so it must be strict */
    conf = -1;
    ASSERT(!uft_format_plugin_d82.probe(b, REGISTRY_PROBE_WINDOW, D80_SIZE, &conf));
    free(b);
}

TEST(d80_and_d82_read_every_track_of_their_zone_table) {
    static const struct { const uft_format_plugin_t *p; const char *file; int heads; }
    cases[] = {
        { &uft_format_plugin_d80, "vice_c1541_8050.d80", 1 },
        { &uft_format_plugin_d82, "vice_c1541_8250.d82", 2 },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        uft_disk_t d; memset(&d, 0, sizeof(d)); d.read_only = true;
        ASSERT(cases[c].p->open(&d, img(cases[c].file), true) == UFT_OK);

        int total = 0;
        for (int head = 0; head < cases[c].heads; head++) {
            for (int cyl = 0; cyl < 77; cyl++) {
                uft_track_t t; memset(&t, 0, sizeof(t));
                ASSERT(cases[c].p->read_track(&d, cyl, head, &t) == UFT_OK);
                int track = cyl + 1;
                size_t want = track <= 39 ? 29u : track <= 53 ? 27u
                            : track <= 64 ? 25u : 23u;
                ASSERT(t.sector_count == want);
                ASSERT(t.sectors[0].data_len == 256);
                total += (int)t.sector_count;
                free_track(&t);
            }
        }
        ASSERT(total == 2083 * cases[c].heads);
        cases[c].p->close(&d);
    }
}

/* ---------------------------------------------------------------- G71 ---- */

TEST(g71_offset_table_resolves_to_real_gcr) {
    size_t len = 0;
    uint8_t *b = slurp(img("vice_c1541_1571.g71"), &len);
    ASSERT(b && len > 12);
    ASSERT(memcmp(b, "GCR-1571", 8) == 0);
    ASSERT(b[8] == 0);                     /* version */
    ASSERT(b[9] == 168);                   /* half-track entries */
    ASSERT((b[10] | (b[11] << 8)) == 7928);/* max track size */

    int conf = -1;
    ASSERT(uft_format_plugin_g71.probe(b, len < REGISTRY_PROBE_WINDOW
                                          ? len : REGISTRY_PROBE_WINDOW, len, &conf));
    ASSERT(conf == 95);                    /* signature is inside the window */
    free(b);
}

TEST(g71_reads_both_sides_as_verbatim_gcr) {
    uft_disk_t d; memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_g71.open(&d, img("vice_c1541_1571.g71"), true) == UFT_OK);

    int formatted = 0;
    size_t smallest = (size_t)-1, largest = 0;
    for (int head = 0; head < 2; head++) {
        for (int cyl = 0; cyl < 35; cyl++) {
            uft_track_t t; memset(&t, 0, sizeof(t));
            ASSERT(uft_format_plugin_g71.read_track(&d, cyl, head, &t) == UFT_OK);
            if (t.status == UFT_TRACK_OK) {
                ASSERT(t.raw_data != NULL && t.raw_size > 0);
                ASSERT(t.encoding == UFT_ENC_GCR_CBM);
                /* GCR sync is a run of 1-bits; every formatted 1571 track
                 * carries several. Empty output would still be "OK" without
                 * this. */
                int ff = 0;
                for (size_t i = 0; i + 1 < t.raw_size; i++)
                    if (t.raw_data[i] == 0xFF && t.raw_data[i + 1] == 0xFF) { ff++; i++; }
                ASSERT(ff >= 5);
                if (t.raw_size < smallest) smallest = t.raw_size;
                if (t.raw_size > largest) largest = t.raw_size;
                formatted++;
            }
            free_track(&t);
        }
    }
    /* 35 tracks on each of the two sides */
    ASSERT(formatted == 70);
    /* Zone speeds make outer tracks longer than inner ones; a reader that
     * returned one fixed length for every track would pass everything above. */
    ASSERT(largest > smallest);
    uft_format_plugin_g71.close(&d);
}

int main(void)
{
    printf("=== CBM containers vs. c1541 reference images (MF-427) ===\n");
    RUN(d67_zone_table_matches_the_c1541_image);
    RUN(d67_bam_and_disk_name_are_where_the_reader_looks);
    RUN(d67_probe_scores_88_only_with_the_whole_file);
    RUN(d80_header_block_sits_at_track_39_sector_0);
    RUN(d82_is_the_double_sided_8050);
    RUN(d80_and_d82_read_every_track_of_their_zone_table);
    RUN(g71_offset_table_resolves_to_real_gcr);
    RUN(g71_reads_both_sides_as_verbatim_gcr);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
