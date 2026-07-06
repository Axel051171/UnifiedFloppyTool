/**
 * @file uft_akai_s900.c
 * @brief Akai S900 / S950 sampler disk — Plugin-B (Sektor-Image, Klasse 3)
 *
 * Akai S900 (DD) and S950 (DD or HD) store samples on 3.5" floppies with a
 * 1024-byte block model — NOT the PC 512-byte layout:
 *   DD  = 80 x 2 x  5 x 1024 =   819,200 bytes  (S900 / S950-DD)
 *   HD  = 80 x 2 x 10 x 1024 = 1,638,400 bytes  (S950-HD)
 *
 * Geometry FACT source: akaiutil (kmi9000), the reverse-engineering reference
 * for the Akai sampler filesystems — the 1024-byte block size / 5-sector DD
 * model is taken from its documented image parameters. Only geometry facts are
 * used here; no akaiutil source code is copied (license-clean).
 *
 * SIZE COLLISION (documented, honest): the DD image (819,200) shares its total
 * size with a Commodore D81 (80 x 40 x 256) and with Korg DSS-1 (also 5 x 1024).
 * There is no Akai header/magic to distinguish by content, so the probe returns
 * a LOWER confidence than the D81 plugin: `.d81` auto-detects as D81, and an
 * Akai image is opened via its own extension (`.akai` / `.s900`) or explicit
 * selection. The value is correct reading of an Akai disk (5/10 x 1024) which
 * the D81 plugin would mis-read as CBM 40 x 256 sectors. The HD size
 * (1,638,400) is unambiguous and probed with higher confidence.
 *
 * Classification: Sektor-Image (Klasse 3). The Akai sample FILESYSTEM
 * (volumes/programs/samples, accessible via akaiutil) is a separate layer and
 * out of scope here (Basis-Niveau / disk-image level only).
 */
#include "uft/uft_format_common.h"

#define AKAI_DD_SIZE    819200
#define AKAI_HD_SIZE    1638400
#define AKAI_TRACKS     80
#define AKAI_HEADS      2
#define AKAI_SS         1024
#define AKAI_DD_SPT     5
#define AKAI_HD_SPT     10

typedef struct { FILE *file; int spt; } akai_pd_t;

static bool akai_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == AKAI_HD_SIZE) { *c = 70; return true; }   /* HD size is distinct */
    /* DD size collides with D81 / Korg — modest confidence below D81 (80). */
    if (fs == AKAI_DD_SIZE) { *c = 40; return true; }
    return false;
}

static uft_error_t akai_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }
    if (sz != AKAI_DD_SIZE && sz != AKAI_HD_SIZE) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    akai_pd_t *p = calloc(1, sizeof(akai_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->spt = (sz == AKAI_HD_SIZE) ? AKAI_HD_SPT : AKAI_DD_SPT;
    disk->plugin_data = p;
    disk->geometry.cylinders = AKAI_TRACKS; disk->geometry.heads = AKAI_HEADS;
    disk->geometry.sectors = p->spt; disk->geometry.sector_size = AKAI_SS;
    disk->geometry.total_sectors = AKAI_TRACKS * AKAI_HEADS * p->spt;
    return UFT_OK;
}

static void akai_close(uft_disk_t *disk) {
    akai_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static long akai_offset(const akai_pd_t *p, int cyl, int head, int sec) {
    return (long)((cyl * AKAI_HEADS + head) * p->spt + sec) * AKAI_SS;
}

static uft_error_t akai_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track) {
    akai_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head < 0 || head >= AKAI_HEADS ||
        cyl < 0 || cyl >= AKAI_TRACKS) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    uint8_t buf[AKAI_SS];
    for (int s = 0; s < p->spt; s++) {
        if (fseek(p->file, akai_offset(p, cyl, head, s), SEEK_SET) != 0)
            return UFT_ERROR_IO;
        if (fread(buf, 1, AKAI_SS, p->file) != AKAI_SS) memset(buf, 0xE5, AKAI_SS);
        uft_format_add_sector(track, (uint8_t)s, buf, AKAI_SS,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t akai_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track) {
    akai_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head < 0 || head >= AKAI_HEADS ||
        cyl < 0 || cyl >= AKAI_TRACKS) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    for (size_t s = 0; s < track->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, akai_offset(p, cyl, head, (int)s), SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[AKAI_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, AKAI_SS); data = pad;
        }
        if (fwrite(data, 1, AKAI_SS, p->file) != AKAI_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_akai_s900_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Sample filesystem", UFT_FEATURE_UNSUPPORTED,
      "Akai volume/program/sample directory not decoded (use akaiutil) — disk-image level only" },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_akai_s900 = {
    .name = "AkaiS900", .description = "Akai S900/S950 Sampler (1024-byte blocks, DD 5x / HD 10x)",
    .extensions = "akai", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = akai_probe, .open = akai_open, .close = akai_close,
    .read_track = akai_read_track, .write_track = akai_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* geometry from akaiutil, no official spec */
    .features = uft_format_plugin_akai_s900_features,
    .feature_count = sizeof(uft_format_plugin_akai_s900_features) / sizeof(uft_format_plugin_akai_s900_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(akai_s900)
