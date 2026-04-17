/**
 * @file uft_northstar.c
 * @brief North Star DOS Plugin-B
 *
 * North Star Horizon / Advantage used hard-sectored 5.25" media with
 * 10 sector holes per track.
 *
 * Geometry:
 *   35 tracks, single sided
 *   10 sectors per track (hard-sector)
 *   SD: 256 bytes/sector  ->  35 x 10 x 256 =  89600
 *   DD: 512 bytes/sector  ->  35 x 10 x 512 = 179200
 *
 * Note: SD size (89600) is the same as JV1 (35x10x256), so confidence
 * is deliberately low (65) to allow other probes to win when they have
 * content-based heuristics.
 *
 * Reference: FluxEngine, North Star MDS-A-D manual
 */
#include "uft/uft_format_common.h"

#define NS_TRACKS   35
#define NS_SPT      10
#define NS_SD_SS    256
#define NS_DD_SS    512
#define NS_SD_SIZE  89600   /* 35 * 10 * 256 */
#define NS_DD_SIZE  179200  /* 35 * 10 * 512 */

typedef struct {
    FILE     *file;
    uint16_t  sector_size;  /* 256 or 512 */
} ns_pd_t;

/* ---- probe ---- */
static bool ns_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == NS_SD_SIZE) { *c = 65; return true; }
    if (fs == NS_DD_SIZE) { *c = 65; return true; }
    return false;
}

/* ---- open ---- */
static uft_error_t ns_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint16_t ss;
    if (sz == NS_SD_SIZE)      ss = NS_SD_SS;
    else if (sz == NS_DD_SIZE) ss = NS_DD_SS;
    else { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    ns_pd_t *p = calloc(1, sizeof(ns_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->sector_size = ss;

    disk->plugin_data = p;
    disk->geometry.cylinders   = NS_TRACKS;
    disk->geometry.heads       = 1;
    disk->geometry.sectors     = NS_SPT;
    disk->geometry.sector_size = ss;
    disk->geometry.total_sectors = NS_TRACKS * NS_SPT;
    return UFT_OK;
}

/* ---- close ---- */
static void ns_close(uft_disk_t *disk) {
    ns_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

/* ---- read_track ---- */
static uft_error_t ns_read_track(uft_disk_t *disk, int cyl, int head,
                                  uft_track_t *track) {
    ns_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    uint16_t ss = p->sector_size;
    long off = (long)cyl * NS_SPT * ss;
    uint8_t buf[NS_DD_SS];  /* large enough for either density */

    for (int s = 0; s < NS_SPT; s++) {
        if (fseek(p->file, off + (long)s * ss, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        if (fread(buf, 1, ss, p->file) != (size_t)ss) {
            memset(buf, 0xE5, ss);
        }
        uft_format_add_sector(track, (uint8_t)s, buf, ss,
                              (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

/* ---- write_track ---- */
static uft_error_t ns_write_track(uft_disk_t *disk, int cyl, int head,
                                   const uft_track_t *track) {
    ns_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    uint16_t ss = p->sector_size;
    long off = (long)cyl * NS_SPT * ss;

    for (size_t s = 0; s < track->sector_count && (int)s < NS_SPT; s++) {
        if (fseek(p->file, off + (long)s * ss, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[NS_DD_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, ss);
            data = pad;
        }
        if (fwrite(data, 1, ss, p->file) != (size_t)ss)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

/* ---- plugin descriptor ---- */
const uft_format_plugin_t uft_format_plugin_northstar = {
    .name = "NorthStar",
    .description = "North Star DOS (hard-sectored)",
    .extensions = "ns;nsi",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe       = ns_probe,
    .open        = ns_open,
    .close       = ns_close,
    .read_track  = ns_read_track,
    .write_track = ns_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(northstar)
