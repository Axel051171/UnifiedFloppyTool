/**
 * @file uft_korg_dss1.c
 * @brief Korg DSS-1 / DSM-1 sampler disk — Plugin-B (Sektor-Image, Klasse 3)
 *
 * Korg DSS-1 (and DSM-1 in DD mode) store samples on DSDD 3.5" floppies with a
 * NON-PC geometry: 80 cylinders x 2 heads x 5 sectors x 1024 bytes = 819,200
 * bytes. Geometry verified against the ChickenSys Translator format reference
 * (chickensys.com/.../korg.html) — NOT guessed.
 *
 * SIZE COLLISION (documented, honest): 819,200 bytes is the same total size as
 * a Commodore D81 (80 x 40 x 256). There is no documented Korg header/magic to
 * distinguish the two by content, so this plugin's probe returns a LOWER
 * confidence than the D81 plugin: a `.d81` auto-detects as D81 (correct for the
 * far more common case), while a Korg image is opened via its own extension
 * (`.dss`) or explicit format selection. The value is correct reading of a Korg
 * disk (5 x 1024 per track/side) which the D81 plugin would mis-read as CBM
 * 40 x 256 sectors.
 *
 * Classification: Sektor-Image (Klasse 3) — decoded sectors, no protection/
 * flux representation. The Korg sample FILESYSTEM (Systems/Programs/Oscillators/
 * Multisounds) is a separate layer and out of scope here (Basis-Niveau only).
 */
#include "uft/uft_format_common.h"

#define KORG_SIZE       819200
#define KORG_TRACKS     80
#define KORG_HEADS      2
#define KORG_SPT        5
#define KORG_SS         1024

typedef struct { FILE *file; } korg_pd_t;

static bool korg_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    /* Size matches, but so does D81 — return a modest confidence BELOW the
     * D81 plugin (80) so D81 wins auto-detect and Korg is extension/manual. */
    if (fs == KORG_SIZE) { *c = 40; return true; }
    return false;
}

static uft_error_t korg_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    korg_pd_t *p = calloc(1, sizeof(korg_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = KORG_TRACKS; disk->geometry.heads = KORG_HEADS;
    disk->geometry.sectors = KORG_SPT; disk->geometry.sector_size = KORG_SS;
    disk->geometry.total_sectors = KORG_TRACKS * KORG_HEADS * KORG_SPT;
    return UFT_OK;
}

static void korg_close(uft_disk_t *disk) {
    korg_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static long korg_offset(int cyl, int head, int sec) {
    return (long)((cyl * KORG_HEADS + head) * KORG_SPT + sec) * KORG_SS;
}

static uft_error_t korg_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track) {
    korg_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head < 0 || head >= KORG_HEADS ||
        cyl < 0 || cyl >= KORG_TRACKS) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    uint8_t buf[KORG_SS];
    for (int s = 0; s < KORG_SPT; s++) {
        if (fseek(p->file, korg_offset(cyl, head, s), SEEK_SET) != 0)
            return UFT_ERROR_IO;
        if (fread(buf, 1, KORG_SS, p->file) != KORG_SS) memset(buf, 0xE5, KORG_SS);
        uft_format_add_sector(track, (uint8_t)s, buf, KORG_SS,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t korg_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track) {
    korg_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head < 0 || head >= KORG_HEADS ||
        cyl < 0 || cyl >= KORG_TRACKS) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    for (size_t s = 0; s < track->sector_count && (int)s < KORG_SPT; s++) {
        if (fseek(p->file, korg_offset(cyl, head, (int)s), SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[KORG_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, KORG_SS); data = pad;
        }
        if (fwrite(data, 1, KORG_SS, p->file) != KORG_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_korg_dss1_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Sample filesystem", UFT_FEATURE_UNSUPPORTED,
      "Korg sample directory (Systems/Programs/Oscillators) not decoded — disk-image level only" },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_korg_dss1 = {
    .name = "KorgDSS1", .description = "Korg DSS-1/DSM-1 Sampler (800K DSDD, 5x1024)",
    .extensions = "dss", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = korg_probe, .open = korg_open, .close = korg_close,
    .read_track = korg_read_track, .write_track = korg_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* geometry from ChickenSys ref, no official spec */
    .features = uft_format_plugin_korg_dss1_features,
    .feature_count = sizeof(uft_format_plugin_korg_dss1_features) / sizeof(uft_format_plugin_korg_dss1_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(korg_dss1)
