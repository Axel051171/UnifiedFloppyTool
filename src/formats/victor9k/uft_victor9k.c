/**
 * @file uft_victor9k.c
 * @brief Victor 9000 / Sirius 1 Plugin-B
 *
 * Victor 9000 uses GCR encoding with variable-speed zones (like C64 but
 * with different zone boundaries).  80 tracks per side, 1 or 2 sides.
 *
 * Zone layout (sectors per track):
 *   Zone 0  tracks  0- 3:  19 spt
 *   Zone 1  tracks  4-15:  18 spt
 *   Zone 2  tracks 16-26:  17 spt
 *   Zone 3  tracks 27-37:  16 spt
 *   Zone 4  tracks 38-48:  15 spt
 *   Zone 5  tracks 49-59:  14 spt
 *   Zone 6  tracks 60-69:  13 spt
 *   Zone 7  tracks 70-79:  12 spt
 *
 * Sector size: 512 bytes.  Total per side: 1224 sectors = 626688 bytes.
 * Raw image: SS = 626688, DS = 1253376.
 *
 * Reference: FluxEngine, MAME Victor 9000 driver
 */
#include "uft/uft_format_common.h"

#define VIC9K_TRACKS    80
#define VIC9K_SS        512
#define VIC9K_SIDE_SECTORS  1224    /* sum of all spt for 80 tracks */
#define VIC9K_SS_SIZE   626688     /* 1224 * 512 */
#define VIC9K_DS_SIZE   1253376    /* 626688 * 2 */

/* Sectors per track, indexed by zone */
static const uint8_t vic9k_zone_table[VIC9K_TRACKS] = {
    19,19,19,19,                        /* zone 0: tracks  0- 3 */
    18,18,18,18,18,18,18,18,18,18,18,18,/* zone 1: tracks  4-15 */
    17,17,17,17,17,17,17,17,17,17,17,   /* zone 2: tracks 16-26 */
    16,16,16,16,16,16,16,16,16,16,16,   /* zone 3: tracks 27-37 */
    15,15,15,15,15,15,15,15,15,15,15,   /* zone 4: tracks 38-48 */
    14,14,14,14,14,14,14,14,14,14,14,   /* zone 5: tracks 49-59 */
    13,13,13,13,13,13,13,13,13,13,      /* zone 6: tracks 60-69 */
    12,12,12,12,12,12,12,12,12,12       /* zone 7: tracks 70-79 */
};

static int vic9k_spt(int track) {
    if (track < 0 || track >= VIC9K_TRACKS) return 0;
    return vic9k_zone_table[track];
}

/** Byte offset of the first sector of (cyl, head) in a raw image */
static long vic9k_track_offset(int cyl, int head) {
    long off = 0;
    /* Sum all previous tracks on this side */
    for (int t = 0; t < cyl; t++)
        off += (long)vic9k_spt(t) * VIC9K_SS;
    if (head == 1)
        off += (long)VIC9K_SIDE_SECTORS * VIC9K_SS;   /* side 1 after side 0 */
    return off;
}

typedef struct {
    FILE    *file;
    uint8_t  heads;
} vic9k_pd_t;

/* ---- probe ---- */
static bool vic9k_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == VIC9K_SS_SIZE || fs == VIC9K_DS_SIZE) {
        *c = 70;
        return true;
    }
    return false;
}

/* ---- open ---- */
static uft_error_t vic9k_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t heads;
    if (sz == VIC9K_SS_SIZE)      heads = 1;
    else if (sz == VIC9K_DS_SIZE) heads = 2;
    else { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    vic9k_pd_t *p = calloc(1, sizeof(vic9k_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->heads = heads;

    disk->plugin_data = p;
    disk->geometry.cylinders = VIC9K_TRACKS;
    disk->geometry.heads     = heads;
    disk->geometry.sectors   = 19;           /* max spt (zone 0) */
    disk->geometry.sector_size = VIC9K_SS;
    disk->geometry.total_sectors = (uint32_t)VIC9K_SIDE_SECTORS * heads;
    return UFT_OK;
}

/* ---- close ---- */
static void vic9k_close(uft_disk_t *disk) {
    vic9k_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

/* ---- read_track ---- */
static uft_error_t vic9k_read_track(uft_disk_t *disk, int cyl, int head,
                                     uft_track_t *track) {
    vic9k_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    int spt = vic9k_spt(cyl);
    long off = vic9k_track_offset(cyl, head);
    uint8_t buf[VIC9K_SS];

    for (int s = 0; s < spt; s++) {
        if (fseek(p->file, off + (long)s * VIC9K_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        if (fread(buf, 1, VIC9K_SS, p->file) != VIC9K_SS) {
            memset(buf, 0xE5, VIC9K_SS);
        }
        uft_format_add_sector(track, (uint8_t)s, buf, VIC9K_SS,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

/* ---- write_track ---- */
static uft_error_t vic9k_write_track(uft_disk_t *disk, int cyl, int head,
                                      const uft_track_t *track) {
    vic9k_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    int spt = vic9k_spt(cyl);
    long off = vic9k_track_offset(cyl, head);

    for (size_t s = 0; s < track->sector_count && (int)s < spt; s++) {
        if (fseek(p->file, off + (long)s * VIC9K_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[VIC9K_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, VIC9K_SS);
            data = pad;
        }
        if (fwrite(data, 1, VIC9K_SS, p->file) != VIC9K_SS)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

/* ---- plugin descriptor ---- */
const uft_format_plugin_t uft_format_plugin_victor9k = {
    .name = "Victor9K",
    .description = "Victor 9000 / Sirius 1 GCR",
    .extensions = "vic;v9k",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe       = vic9k_probe,
    .open        = vic9k_open,
    .close       = vic9k_close,
    .read_track  = vic9k_read_track,
    .write_track = vic9k_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(victor9k)
