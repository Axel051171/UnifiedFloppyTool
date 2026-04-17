/**
 * @file uft_micropolis.c
 * @brief Micropolis MetaFloppy Plugin-B
 *
 * Micropolis 100 tpi hard-sectored format:
 *   16 sectors/track (hard-sector holes)
 *   77 tracks, 1 or 2 sides
 *   Sector: 266 bytes on media (256 data + 10 overhead),
 *           but raw images store only the 256-byte data payload.
 *
 * Raw image sizes:
 *   SS: 77 x 16 x 256 = 315392
 *   DS: 77 x 16 x 256 x 2 = 630784
 *
 * Used by: Vector Graphic, Exidy Sorcerer, Processor Technology,
 *          North Star (some), and various S-100 bus systems.
 *
 * Reference: FluxEngine, MAME Micropolis driver
 */
#include "uft/uft_format_common.h"

#define MPLS_TRACKS     77
#define MPLS_SPT        16
#define MPLS_SS         256     /* data payload per sector in raw image */
#define MPLS_SS_SIZE    315392  /* 77 * 16 * 256 */
#define MPLS_DS_SIZE    630784  /* 315392 * 2 */

typedef struct {
    FILE    *file;
    uint8_t  heads;
} mpls_pd_t;

/* ---- probe ---- */
static bool mpls_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == MPLS_SS_SIZE || fs == MPLS_DS_SIZE) {
        *c = 70;
        return true;
    }
    return false;
}

/* ---- open ---- */
static uft_error_t mpls_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t heads;
    if (sz == MPLS_SS_SIZE)      heads = 1;
    else if (sz == MPLS_DS_SIZE) heads = 2;
    else { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    mpls_pd_t *p = calloc(1, sizeof(mpls_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->heads = heads;

    disk->plugin_data = p;
    disk->geometry.cylinders   = MPLS_TRACKS;
    disk->geometry.heads       = heads;
    disk->geometry.sectors     = MPLS_SPT;
    disk->geometry.sector_size = MPLS_SS;
    disk->geometry.total_sectors = (uint32_t)MPLS_TRACKS * heads * MPLS_SPT;
    return UFT_OK;
}

/* ---- close ---- */
static void mpls_close(uft_disk_t *disk) {
    mpls_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

/* ---- read_track ---- */
static uft_error_t mpls_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track) {
    mpls_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    long off = (long)((size_t)cyl * p->heads + head) * MPLS_SPT * MPLS_SS;
    uint8_t buf[MPLS_SS];

    for (int s = 0; s < MPLS_SPT; s++) {
        if (fseek(p->file, off + (long)s * MPLS_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        if (fread(buf, 1, MPLS_SS, p->file) != MPLS_SS) {
            memset(buf, 0xE5, MPLS_SS);
        }
        uft_format_add_sector(track, (uint8_t)s, buf, MPLS_SS,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

/* ---- write_track ---- */
static uft_error_t mpls_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track) {
    mpls_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    long off = (long)((size_t)cyl * p->heads + head) * MPLS_SPT * MPLS_SS;

    for (size_t s = 0; s < track->sector_count && (int)s < MPLS_SPT; s++) {
        if (fseek(p->file, off + (long)s * MPLS_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[MPLS_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, MPLS_SS);
            data = pad;
        }
        if (fwrite(data, 1, MPLS_SS, p->file) != MPLS_SS)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

/* ---- plugin descriptor ---- */
const uft_format_plugin_t uft_format_plugin_micropolis = {
    .name = "Micropolis",
    .description = "Micropolis MetaFloppy (hard-sectored)",
    .extensions = "mpls;micr",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe       = mpls_probe,
    .open        = mpls_open,
    .close       = mpls_close,
    .read_track  = mpls_read_track,
    .write_track = mpls_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(micropolis)
