/**
 * @file uft_po.c
 * @brief PO (Apple II ProDOS Order) Plugin-B
 *
 * Same as DO but ProDOS sector order (non-interleaved).
 * 35 tracks × 16 sectors × 256 bytes = 143,360 bytes.
 *
 * Why the probe cannot separate this from DO — and why that is the correct
 * answer rather than a gap — is written out in src/formats/do/uft_do.c
 * (MF-463). Short version: the two orders differ only in sectors 1..14, and
 * the structure that would decide it sits past the probe buffer.
 */
#include "uft/uft_format_common.h"
#include "uft/formats/apple/uft_apple_order.h"

#define PO_SIZE   143360
#define PO_TRACKS 35
#define PO_SPT    16
#define PO_SS     256

typedef struct { FILE *file; } po_pd_t;

/* MF-724: Gegenstueck zu `do_probe()`. Der Verzeichniskopf auf 0x400 ist
 * der einzige im Sondenpuffer erreichbare Beleg fuer die Anordnung —
 * Begruendung und Quellen in `uft/formats/apple/uft_apple_order.h`. */
static bool po_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    if (fs != PO_SIZE) return false;
    *c = uft_a2_has_prodos_voldir(d, s) ? UFT_A2_CONF_EVIDENZ
                                        : UFT_A2_CONF_UNKLAR;
    return true;
}

static uft_error_t po_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    po_pd_t *p = calloc(1, sizeof(po_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = PO_TRACKS; disk->geometry.heads = 1;
    disk->geometry.sectors = PO_SPT; disk->geometry.sector_size = PO_SS;
    disk->geometry.total_sectors = PO_TRACKS * PO_SPT;
    return UFT_OK;
}

static void po_close(uft_disk_t *disk) {
    po_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t po_read_track(uft_disk_t *disk, int cyl, int head, uft_track_t *track) {
    po_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (cyl < 0 || cyl >= PO_TRACKS) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    uint8_t buf[PO_SS];
    for (int s = 0; s < PO_SPT; s++) {
        if (fseek(p->file, (long)(cyl * PO_SPT + s) * PO_SS, SEEK_SET) != 0) return UFT_ERROR_IO;
        /* Not read means not there — never hand out fill as if it were data
         * (MF-463; same change in uft_do.c). */
        if (fread(buf, 1, PO_SS, p->file) != PO_SS) break;
        /* Apple sectors are 0..15 (ARCH-20) */
        uft_format_add_sector_with_id(track, (uint8_t)s, buf, PO_SS, (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

static uft_error_t po_write_track(uft_disk_t *disk, int cyl, int head,
                                   const uft_track_t *track) {
    po_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (cyl < 0 || cyl >= PO_TRACKS) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    for (size_t s = 0; s < track->sector_count && (int)s < PO_SPT; s++) {
        if (fseek(p->file, (long)(cyl * PO_SPT + (int)s) * PO_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[PO_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, PO_SS); data = pad;
        }
        if (fwrite(data, 1, PO_SS, p->file) != PO_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_po_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_po = {
    .name = "PO", .description = "Apple II ProDOS Order",
    .extensions = "po;dsk", .format = UFT_FORMAT_PO,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = po_probe, .open = po_open, .close = po_close,
    .read_track = po_read_track, .write_track = po_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_po_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_po_features) / sizeof(uft_format_plugin_po_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(po)
