/**
 * @file uft_lisa_twiggy.c
 * @brief Apple Lisa Twiggy / FileWare disk — Plugin-B (dekodiertes Sektor-Image)
 *
 * The Twiggy (Apple 871 / FileWare) is a 5.25" double-sided GCR disk used on the
 * Apple Lisa. It is fundamentally a FLUX-level format (Klasse 1): the drive
 * varies motor speed 218-320 RPM (ZCAV) so outer tracks hold more sectors —
 * that variable-speed timing can only be preserved at the flux level. This
 * plugin handles the DECODED sector image (the byte view a Lisa tool exports),
 * NOT the flux; the variable-speed timing is therefore lost in this
 * representation (documented in the feature matrix).
 *
 * Verified geometry (per-track sector counts by speed zone — 46 tracks/side,
 * 2 sides, 512 data bytes/sector; total 1702 sectors = 871,424 bytes):
 *   tracks  0- 3 : 22 sectors   (4)
 *   tracks  4-10 : 21 sectors   (7)
 *   tracks 11-16 : 20 sectors   (6)
 *   tracks 17-22 : 19 sectors   (6)
 *   tracks 23-28 : 18 sectors   (6)
 *   tracks 29-34 : 17 sectors   (6)
 *   tracks 35-41 : 16 sectors   (7)
 *   tracks 42-45 : 15 sectors   (4)
 *   => 4*22+7*21+6*20+6*19+6*18+6*17+7*16+4*15 = 851/side, x2 = 1702.
 * Source: Apple FileWare docs (Wikipedia/archiveteam), consistent with the
 * 871,424-byte capacity. The primary Apple spec (bitsavers Twiggy_Specs.pdf)
 * exists but was not machine-parsed here — spec_status REVERSE_ENGINEERED.
 *
 * Image layout convention (documented, corpus-verification pending): sectors
 * ordered cylinder-major, side-minor — for each cylinder, head 0's sectors then
 * head 1's. The 12/20-byte per-sector TAG bytes are metadata not present in the
 * 512-byte data image and are out of scope. Byte-exact correctness against a
 * real Twiggy image still needs a ground-truth reference (open point).
 */
#include "uft/uft_format_common.h"

#define TWIGGY_TRACKS   46
#define TWIGGY_HEADS    2
#define TWIGGY_SS       512
#define TWIGGY_SIZE     871424   /* 1702 * 512 */

/* Sectors per track (index = cylinder 0..45), verified zone table. */
static const uint8_t twiggy_spt[TWIGGY_TRACKS] = {
    22,22,22,22,                      /*  0-3  */
    21,21,21,21,21,21,21,             /*  4-10 */
    20,20,20,20,20,20,                /* 11-16 */
    19,19,19,19,19,19,                /* 17-22 */
    18,18,18,18,18,18,                /* 23-28 */
    17,17,17,17,17,17,                /* 29-34 */
    16,16,16,16,16,16,16,             /* 35-41 */
    15,15,15,15                       /* 42-45 */
};

typedef struct { FILE *file; } twiggy_pd_t;

static bool twiggy_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == TWIGGY_SIZE) { *c = 85; return true; }  /* size is distinct (no collision) */
    return false;
}

static uft_error_t twiggy_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    twiggy_pd_t *p = calloc(1, sizeof(twiggy_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = TWIGGY_TRACKS;
    disk->geometry.heads = TWIGGY_HEADS;
    disk->geometry.sectors = twiggy_spt[0];  /* max (zoned) — real per-track via read */
    disk->geometry.sector_size = TWIGGY_SS;
    uint32_t total = 0;
    for (int t = 0; t < TWIGGY_TRACKS; t++) total += twiggy_spt[t];
    disk->geometry.total_sectors = total * TWIGGY_HEADS;
    return UFT_OK;
}

static void twiggy_close(uft_disk_t *disk) {
    twiggy_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

/* Byte offset of (cyl, head, sector 0): cylinder-major, side-minor. */
static long twiggy_track_offset(int cyl, int head) {
    long sectors = 0;
    for (int c = 0; c < cyl; c++) sectors += (long)twiggy_spt[c] * TWIGGY_HEADS;
    if (head > 0) sectors += twiggy_spt[cyl];
    return sectors * TWIGGY_SS;
}

static uft_error_t twiggy_read_track(uft_disk_t *disk, int cyl, int head,
                                      uft_track_t *track) {
    twiggy_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head < 0 || head >= TWIGGY_HEADS ||
        cyl < 0 || cyl >= TWIGGY_TRACKS) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    int spt = twiggy_spt[cyl];
    long off = twiggy_track_offset(cyl, head);
    uint8_t buf[TWIGGY_SS];
    for (int s = 0; s < spt; s++) {
        if (fseek(p->file, off + (long)s * TWIGGY_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        if (fread(buf, 1, TWIGGY_SS, p->file) != TWIGGY_SS) memset(buf, 0xE5, TWIGGY_SS);
        uft_format_add_sector(track, (uint8_t)s, buf, TWIGGY_SS,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t twiggy_write_track(uft_disk_t *disk, int cyl, int head,
                                       const uft_track_t *track) {
    twiggy_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head < 0 || head >= TWIGGY_HEADS ||
        cyl < 0 || cyl >= TWIGGY_TRACKS) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    int spt = twiggy_spt[cyl];
    long off = twiggy_track_offset(cyl, head);
    for (size_t s = 0; s < track->sector_count && (int)s < spt; s++) {
        if (fseek(p->file, off + (long)s * TWIGGY_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[TWIGGY_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, TWIGGY_SS); data = pad;
        }
        if (fwrite(data, 1, TWIGGY_SS, p->file) != TWIGGY_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_lisa_twiggy_features[] = {
    { "Read (decoded sectors)", UFT_FEATURE_SUPPORTED, NULL },
    { "Write (decoded sectors)", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux / variable-speed timing", UFT_FEATURE_UNSUPPORTED,
      "Twiggy is a variable-speed (ZCAV) flux format; the decoded sector image does not preserve motor-speed timing — flux-level capture (Klasse 1) required for that" },
    { "Tag bytes (12/20 per sector)", UFT_FEATURE_UNSUPPORTED,
      "per-sector tag bytes are metadata, not in the 512-byte data image" },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_lisa_twiggy = {
    .name = "LisaTwiggy", .description = "Apple Lisa Twiggy/FileWare (871K, zoned GCR, decoded sectors)",
    .extensions = "twig", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = twiggy_probe, .open = twiggy_open, .close = twiggy_close,
    .read_track = twiggy_read_track, .write_track = twiggy_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* zone table from FileWare docs; primary Twiggy_Specs.pdf not machine-parsed */
    .features = uft_format_plugin_lisa_twiggy_features,
    .feature_count = sizeof(uft_format_plugin_lisa_twiggy_features) / sizeof(uft_format_plugin_lisa_twiggy_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(lisa_twiggy)
