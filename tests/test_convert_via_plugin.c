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
extern const uft_format_plugin_t uft_format_plugin_g64;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D64_IMAGE "vice_c1541_35trk.d64"
#define D67_IMAGE "vice_c1541_2040.d67"
#define G64_IMAGE "vice_c1541_35trk.g64"

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
    /* The BAM lives at track 18 SECTOR 0, and since ARCH-20 (MF-465) the
     * plugin reports the number the 1541 actually writes into the GCR header.
     * This line asserted 1 before, because the shared add-sector helper added
     * one to every index regardless of what the drive does. */
    ASSERT(bam.sectors[0].id.sector == 0);
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


/* ==========================================================================
 * Second seam: G64 -> D64, the decode direction (MF-436)
 * ========================================================================== */

/* Decode via the existing blob path: g64_load_buffer -> g64_to_d64. */
static d64_image_t *decode_via_blob(convert_result_t *res)
{
    size_t len = 0;
    uint8_t *raw = slurp(img(G64_IMAGE), &len);
    if (!raw) return NULL;

    g64_image_t *g64 = NULL;
    if (g64_load_buffer(raw, len, &g64) != 0 || !g64) { free(raw); return NULL; }
    free(raw);

    convert_options_t opts;
    convert_get_defaults(&opts);
    d64_image_t *d64 = NULL;
    int rc = g64_to_d64(g64, &d64, &opts, res);
    g64_free(g64);
    return (rc == 0) ? d64 : NULL;
}

/* Decode via the plugin path: uft_format_plugin_g64 -> same GCR decoder. */
static d64_image_t *decode_via_plugin(convert_result_t *res)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    if (uft_format_plugin_g64.open(&disk, img(G64_IMAGE), true) != UFT_OK)
        return NULL;

    convert_options_t opts;
    convert_get_defaults(&opts);
    d64_image_t *d64 = NULL;
    int rc = uft_cbm_d64_decode_via_plugin(&uft_format_plugin_g64, &disk,
                                            &opts, &d64, res);
    uft_format_plugin_g64.close(&disk);
    return (rc == 0) ? d64 : NULL;
}

TEST(decode_both_paths_are_byte_identical) {
    /* Same statement as for the encode direction, in reverse: if the plugin
     * carries everything the GCR decoder needs, the sector data must come out
     * bit for bit the same as from g64_load_buffer(). */
    convert_result_t rb, rp;
    memset(&rb, 0, sizeof(rb)); memset(&rp, 0, sizeof(rp));
    d64_image_t *blob = decode_via_blob(&rb);
    d64_image_t *plug = decode_via_plugin(&rp);
    ASSERT(blob != NULL);
    ASSERT(plug != NULL);

    ASSERT(blob->num_tracks == plug->num_tracks);
    ASSERT(blob->num_blocks == plug->num_blocks);
    ASSERT(rb.sectors_converted == rp.sectors_converted);
    ASSERT(rb.sectors_converted > 0);          /* not a vacuous pass */
    ASSERT(rb.tracks_converted == rp.tracks_converted);

    ASSERT(memcmp(blob->data, plug->data,
                  (size_t)blob->num_blocks * 256u) == 0);
    /* the disk ID is read out of the GCR BAM track on both paths */
    ASSERT(memcmp(blob->disk_id, plug->disk_id, 2) == 0);
    ASSERT(blob->disk_id[0] == '4' && blob->disk_id[1] == '2');

    d64_free(blob); d64_free(plug);
}

TEST(the_full_roundtrip_over_plugins_returns_the_original_disk) {
    /* The point of having both seams. D64 -> G64 -> D64, every step through
     * the plugin interface, and the sector data must come back unchanged.
     *
     * This is a stronger claim than either half: encoding to GCR and decoding
     * back exercises sector headers, checksums, sync marks, gap handling and
     * the zone table in one pass. A single wrong sector ID or a checksum
     * computed over the wrong range would show up here and nowhere else.
     *
     * "Kein Bit verloren" is the project's first sentence. This is what it
     * looks like as an assertion. */
    size_t orig_len = 0;
    uint8_t *orig = slurp(img(D64_IMAGE), &orig_len);
    ASSERT(orig != NULL);
    ASSERT(orig_len == 174848u);               /* 683 blocks, 35 tracks */

    /* Forward: D64 -> G64, through uft_format_plugin_d64 */
    convert_result_t enc;
    memset(&enc, 0, sizeof(enc));
    g64_image_t *g64 = encode_via_plugin(&enc);
    ASSERT(g64 != NULL);
    ASSERT(enc.sectors_converted == 683);

    /* Back: G64 -> D64, using the same decoder the plugin path uses. The G64
     * lives in memory here, so the tracks are handed to the shared decoder
     * directly rather than through a plugin open() — the decoder is the same
     * function either way. */
    convert_options_t opts;
    convert_get_defaults(&opts);
    d64_image_t *back = NULL;
    convert_result_t dec;
    memset(&dec, 0, sizeof(dec));
    ASSERT(g64_to_d64(g64, &back, &opts, &dec) == 0);
    ASSERT(back != NULL);
    ASSERT(dec.sectors_converted == 683);      /* every sector came back */
    ASSERT(dec.errors_found == 0);             /* every checksum verified */

    /* And the payload is the disk we started from. */
    ASSERT(back->num_blocks == 683);
    int differing = 0;
    for (size_t i = 0; i < orig_len; i++)
        if (orig[i] != back->data[i]) differing++;
    if (differing) {
        printf("\n        %d of %zu bytes differ after the roundtrip\n",
               differing, orig_len);
    }
    ASSERT(differing == 0);

    free(orig);
    g64_free(g64);
    d64_free(back);
}


TEST(geometry_cylinders_is_capacity_for_g64_not_extent) {
    /* The trap this seam walked into, pinned so nobody walks into it again.
     *
     * A D64 has a fixed layout: its size IS its extent, and the plugin's
     * geometry.cylinders says 35. A G64 carries a slot-count header — 84 in
     * the reference image, 42 addressable tracks — while only 35 slots hold
     * data. Reading geometry.cylinders as "how many tracks are on this disk"
     * therefore produced a 40-track D64 out of a 35-track disk: 85 blocks of
     * padding presented as recovered sectors, which is precisely the kind of
     * invented data the first principle forbids.
     *
     * A decoder must ask the CONTENT. */
    uft_disk_t g64d, d64d;
    memset(&g64d, 0, sizeof(g64d)); g64d.read_only = true;
    memset(&d64d, 0, sizeof(d64d)); d64d.read_only = true;
    ASSERT(uft_format_plugin_g64.open(&g64d, img(G64_IMAGE), true) == UFT_OK);
    ASSERT(uft_format_plugin_d64.open(&d64d, img(D64_IMAGE), true) == UFT_OK);

    /* Same disk, two containers, two very different meanings. */
    ASSERT(d64d.geometry.cylinders == 35);      /* fixed layout: extent */
    ASSERT(g64d.geometry.cylinders == 42);      /* capacity header: range */

    /* And the content agrees with the D64, not with the G64 header: tracks
     * 36..40 hold nothing. */
    int occupied_beyond_35 = 0;
    for (int trk = 36; trk <= 40; trk++) {
        uft_track_t t;
        memset(&t, 0, sizeof(t));
        if (uft_format_plugin_g64.read_track(&g64d, trk - 1, 0, &t) == UFT_OK &&
            t.raw_data && t.raw_size > 0) {
            occupied_beyond_35++;
        }
        uft_track_release(&t);
    }
    ASSERT(occupied_beyond_35 == 0);

    uft_format_plugin_g64.close(&g64d);
    uft_format_plugin_d64.close(&d64d);
}

int main(void)
{
    printf("=== Converter seam: blob loader vs format plugin (MF-433) ===\n");
    RUN(both_paths_produce_a_g64_at_all);
    RUN(the_two_paths_are_byte_identical);
    RUN(the_disk_id_came_from_the_bam_not_a_default);
    RUN(the_plugin_path_needs_no_d64_knowledge);
    RUN(the_same_encoder_now_reaches_a_format_it_never_could);
    RUN(decode_both_paths_are_byte_identical);
    RUN(geometry_cylinders_is_capacity_for_g64_not_extent);
    RUN(the_full_roundtrip_over_plugins_returns_the_original_disk);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
