/**
 * @file uft_d67.c
 * @brief D67 (Commodore 2040/4040) Plugin-B
 *
 * D67 is a Commodore 2040/4040 disk image, similar to D64 but with
 * different sectors-per-track zones:
 *   Tracks  1-17: 21 spt (zone 3)
 *   Tracks 18-24: 20 spt (zone 2 -- differs from D64's 19!)
 *   Tracks 25-30: 18 spt (zone 1)
 *   Tracks 31-35: 17 spt (zone 0)
 *   Total: 690 sectors x 256 bytes = 176640 bytes
 *
 * BAM is at track 18, sector 0 (same as D64/2040 convention).
 *
 * Reference: VICE emulator, Commodore 2040/4040 technical docs
 */
#include "uft/uft_format_common.h"

#define D67_FILE_SIZE   176640
#define D67_TRACKS      35
#define D67_TOTAL_SEC   690
#define D67_SS          256

/* SPT table: index by (track-1), track range 1..35 */
static int d67_spt(int track_1based)
{
    if (track_1based <=  0) return 0;
    if (track_1based <= 17) return 21;
    if (track_1based <= 24) return 20;
    if (track_1based <= 30) return 18;
    if (track_1based <= 35) return 17;
    return 0;
}

/* Compute file offset for track (1-based) sector 0 */
static long d67_track_offset(int track_1based)
{
    long off = 0;
    for (int t = 1; t < track_1based; t++)
        off += d67_spt(t) * D67_SS;
    return off;
}

typedef struct { FILE *file; } d67_pd_t;

static bool d67_probe(const uint8_t *data, size_t size, size_t file_size,
                       int *confidence)
{
    (void)data; (void)size;
    if (file_size != D67_FILE_SIZE) return false;

    *confidence = 75;

    /* Check BAM at track 18 sector 0:
     * Byte 0: track pointer to directory (should be 18)
     * Byte 1: sector pointer (should be 1)
     * Byte 2: DOS format type (typically 'A' = 0x41 for 2040) */
    if (size >= (size_t)(d67_track_offset(18) + 3)) {
        long bam_off = d67_track_offset(18);
        if (data[bam_off] == 18 && data[bam_off + 1] == 1) {
            *confidence = 88;
        }
    }
    return true;
}

static uft_error_t d67_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    if ((size_t)fs != D67_FILE_SIZE) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    d67_pd_t *p = calloc(1, sizeof(d67_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;

    disk->plugin_data = p;
    disk->geometry.cylinders = D67_TRACKS;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 21; /* max SPT */
    disk->geometry.sector_size = D67_SS;
    disk->geometry.total_sectors = D67_TOTAL_SEC;
    return UFT_OK;
}

static void d67_close(uft_disk_t *disk)
{
    d67_pd_t *p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t d67_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    d67_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;

    int track_1based = cyl + 1;
    int spt = d67_spt(track_1based);
    if (spt == 0) return UFT_ERROR_INVALID_ARG;

    uft_track_init(track, cyl, head);

    long base = d67_track_offset(track_1based);
    uint8_t buf[D67_SS];

    for (int s = 0; s < spt; s++) {
        long off = base + (long)s * D67_SS;
        if (fseek(p->file, off, SEEK_SET) != 0) {
            memset(buf, 0xE5, D67_SS);
        } else if (fread(buf, 1, D67_SS, p->file) != D67_SS) {
            memset(buf, 0xE5, D67_SS);
        }
        uft_format_add_sector(track, (uint8_t)s, buf, D67_SS,
                              (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

static uft_error_t d67_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track)
{
    d67_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    int track_1based = cyl + 1;
    int spt = d67_spt(track_1based);
    if (spt == 0) return UFT_ERROR_INVALID_ARG;

    long base = d67_track_offset(track_1based);

    for (size_t s = 0; s < track->sector_count && (int)s < spt; s++) {
        long off = base + (long)s * D67_SS;
        if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;

        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[D67_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, D67_SS);
            data = pad;
        }
        if (fwrite(data, 1, D67_SS, p->file) != D67_SS)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d67 = {
    .name = "D67", .description = "Commodore 2040/4040 (D67)",
    .extensions = "d67", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d67_probe, .open = d67_open, .close = d67_close,
    .read_track = d67_read_track, .write_track = d67_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(d67)
