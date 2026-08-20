/**
 * @file test_convert_via_plugin.c
 * @brief The converter seam: same G64 whether sectors come from the blob
 *        loader or from the format plugin (MF-433).
 *
 * The converter has never touched the plugin registry. dispatch_conversion()
 * in uft_format_convert_dispatch.c is a hand-written chain of
 * `if (src == X && dst == Y) return convert_x_to_y(...)`, and each of those
 * uses its own loader — d64_load_buffer() here, not uft_format_plugin_d64.
 * The consequence is two parallel format layers: 88 plugins for probing and
 * the GUI, roughly twenty hand-wired pairs for conversion, sharing no code.
 * That is why a bug fixed in one D64 reader stays in the other five, and why
 * a T1b tier on the plugin says nothing about what a conversion actually runs.
 *
 * This is the first plank across that gap, and its job is to prove the plank
 * holds weight rather than to be impressive: uft_cbm_g64_encode_via_plugin()
 * uses the SAME GCR encoder as d64_to_g64(), but takes its sectors from
 * plugin->read_track(). If the two outputs are byte-identical on a real
 * reference image, then the plugin interface carries everything the encoder
 * needs, and the remaining pair functions can follow without guesswork.
 *
 * Reference image: tests/corpus_free/vice_c1541_35trk.d64, produced by VICE
 * c1541 (T1b, tests/corpus_manifest/manifest.json).
 *
 * Byte-identity is the assertion because it is the one that cannot be argued
 * with. "Both produce a valid G64" would pass with two different encoders.
 */

#include "uft/formats/c64/uft_d64_g64.h"
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

extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_d67;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D64_IMAGE "vice_c1541_35trk.d64"
#define D67_IMAGE "vice_c1541_2040.d67"

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

/* Encode via the existing blob path: d64_load_buffer -> d64_to_g64. */
static g64_image_t *encode_via_blob(convert_result_t *res)
{
    size_t len = 0;
    uint8_t *raw = slurp(img(D64_IMAGE), &len);
    if (!raw) return NULL;

    d64_image_t *d64 = NULL;
    if (d64_load_buffer(raw, len, &d64) != 0 || !d64) { free(raw); return NULL; }
    free(raw);

    convert_options_t opts;
    convert_get_defaults(&opts);
    g64_image_t *g64 = NULL;
    int rc = d64_to_g64(d64, &g64, &opts, res);
    d64_free(d64);
    return (rc == 0) ? g64 : NULL;
}

/* Encode via the plugin path: uft_format_plugin_d64 -> same GCR encoder. */
static g64_image_t *encode_via_plugin(convert_result_t *res)
{
    static uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    if (uft_format_plugin_d64.open(&disk, img(D64_IMAGE), true) != UFT_OK)
        return NULL;

    convert_options_t opts;
    convert_get_defaults(&opts);
    g64_image_t *g64 = NULL;
    int rc = uft_cbm_g64_encode_via_plugin(&uft_format_plugin_d64, &disk,
                                            &opts, &g64, res);
    uft_format_plugin_d64.close(&disk);
    return (rc == 0) ? g64 : NULL;
}

TEST(both_paths_produce_a_g64_at_all) {
    convert_result_t rb, rp;
    memset(&rb, 0, sizeof(rb)); memset(&rp, 0, sizeof(rp));
    g64_image_t *blob = encode_via_blob(&rb);
    g64_image_t *plug = encode_via_plugin(&rp);
    ASSERT(blob != NULL);
    ASSERT(plug != NULL);

    /* A 35-track image: both must see all of it. If either reported zero the
     * byte comparison below would pass vacuously. */
    ASSERT(rb.tracks_converted == 35);
    ASSERT(rp.tracks_converted == 35);
    ASSERT(rb.sectors_converted == 683);   /* 1541 total, 35 tracks */
    ASSERT(rp.sectors_converted == 683);

    g64_free(blob); g64_free(plug);
}

TEST(the_two_paths_are_byte_identical) {
    convert_result_t rb, rp;
    memset(&rb, 0, sizeof(rb)); memset(&rp, 0, sizeof(rp));
    g64_image_t *blob = encode_via_blob(&rb);
    g64_image_t *plug = encode_via_plugin(&rp);
    ASSERT(blob && plug);

    ASSERT(blob->num_tracks == plug->num_tracks);
    ASSERT(blob->max_track_size == plug->max_track_size);
    ASSERT(blob->version == plug->version);

    int compared = 0;
    for (int ht = 0; ht < G64_MAX_TRACKS; ht++) {
        ASSERT(blob->tracks[ht].length == plug->tracks[ht].length);
        ASSERT(blob->tracks[ht].speed == plug->tracks[ht].speed);
        ASSERT((blob->track_data[ht] == NULL) == (plug->track_data[ht] == NULL));
        if (blob->track_data[ht] && blob->tracks[ht].length) {
            ASSERT(memcmp(blob->track_data[ht], plug->track_data[ht],
                          blob->tracks[ht].length) == 0);
            compared++;
        }
    }
    ASSERT(compared == 35);   /* every track really was compared */

    g64_free(blob); g64_free(plug);
}

TEST(the_disk_id_came_from_the_bam_not_a_default) {
    /* The disk ID goes into every sector-header checksum, so a wrong one
     * corrupts the whole image in a way that still looks structurally valid.
     * The plugin path reads it through read_track(); this pins that it found
     * the real value rather than falling back to the '0','0' default.
     *
     * The corpus image was formatted `c1541 -format "uftcorpus,42"`, so the
     * BAM at track 18 sector 0 offset 0xA2 holds '4','2'. */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_d64.open(&disk, img(D64_IMAGE), true) == UFT_OK);

    uft_track_t bam;
    memset(&bam, 0, sizeof(bam));
    ASSERT(uft_format_plugin_d64.read_track(&disk, 17, 0, &bam) == UFT_OK);
    ASSERT(bam.sector_count > 0);
    ASSERT(bam.sectors[0].id.sector == 1);          /* IDs are 1-based */
    ASSERT(bam.sectors[0].data != NULL);
    ASSERT(bam.sectors[0].data[0xA2] == '4');
    ASSERT(bam.sectors[0].data[0xA3] == '2');

    uft_track_release(&bam);
    uft_format_plugin_d64.close(&disk);
}

TEST(the_plugin_path_needs_no_d64_knowledge) {
    /* The point of the seam. The caller hands over a plugin and an open disk;
     * track count comes from disk->geometry, sector count from the track the
     * plugin returns, disk ID from the BAM through the same plugin. No zone
     * table, no file layout, no 683-vs-768 branch on the caller's side.
     *
     * Asserting it structurally: the geometry the plugin reports is what the
     * encoder used, and it matches the reference image. */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_d64.open(&disk, img(D64_IMAGE), true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 35);
    ASSERT(disk.geometry.total_sectors == 683);
    ASSERT(disk.geometry.sector_size == 256);
    uft_format_plugin_d64.close(&disk);
}


TEST(the_same_encoder_now_reaches_a_format_it_never_could) {
    /* The payoff, and the reason the seam is worth anything.
     *
     * D67 (Commodore 2040/4040) has no pair function anywhere in the
     * converter — dispatch_conversion() has no `if (src == UFT_FORMAT_D67)`
     * branch, and writing one would mean a second copy of the GCR encoder.
     * Through the plugin it needs nothing new: the zone table comes from the
     * D67 plugin's read_track(), the disk ID from its BAM.
     *
     * What this pins down is the zone difference that DEFINES D67 against D64:
     * tracks 18-24 carry 20 sectors where a 1541 carries 19. If the encoder
     * had kept any 1541 assumption about sector counts, those seven tracks
     * would come out with 19. */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_d67.open(&disk, img(D67_IMAGE), true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 35);
    ASSERT(disk.geometry.total_sectors == 690);   /* 1541 has 683 */

    convert_options_t opts;
    convert_get_defaults(&opts);
    convert_result_t res;
    memset(&res, 0, sizeof(res));
    g64_image_t *g64 = NULL;
    ASSERT(uft_cbm_g64_encode_via_plugin(&uft_format_plugin_d67, &disk,
                                          &opts, &g64, &res) == 0);
    ASSERT(g64 != NULL);
    ASSERT(res.tracks_converted == 35);
    ASSERT(res.sectors_converted == 690);         /* all of them, not 683 */

    /* Every encoded track must carry GCR sync (runs of 1-bits). An encoder
     * that produced only gap fill would satisfy every count above. */
    int with_sync = 0;
    for (int ht = 0; ht < G64_MAX_TRACKS; ht++) {
        if (!g64->track_data[ht] || !g64->tracks[ht].length) continue;
        int ff = 0;
        for (uint16_t i = 0; i + 1 < g64->tracks[ht].length; i++)
            if (g64->track_data[ht][i] == 0xFF &&
                g64->track_data[ht][i + 1] == 0xFF) { ff++; i++; }
        if (ff >= 5) with_sync++;
    }
    ASSERT(with_sync == 35);

    g64_free(g64);
    uft_format_plugin_d67.close(&disk);
}

int main(void)
{
    printf("=== Converter seam: blob loader vs format plugin (MF-433) ===\n");
    RUN(both_paths_produce_a_g64_at_all);
    RUN(the_two_paths_are_byte_identical);
    RUN(the_disk_id_came_from_the_bam_not_a_default);
    RUN(the_plugin_path_needs_no_d64_knowledge);
    RUN(the_same_encoder_now_reaches_a_format_it_never_could);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
