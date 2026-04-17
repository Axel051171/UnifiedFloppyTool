/**
 * @file uft_scl_plugin.c
 * @brief SCL (ZX Spectrum TR-DOS container) Plugin-B
 *
 * SCL is a container that stores TR-DOS files with a directory.
 * Header: "SINCLAIR" (8 bytes) + file_count(1) + entries(14 bytes each)
 * Data follows after the directory.
 */
#include "uft/uft_format_common.h"

#define SCL_MAGIC       "SINCLAIR"
#define SCL_MAGIC_LEN   8
#define SCL_ENTRY_SIZE  14
#define SCL_SECTOR_SIZE 256

typedef struct {
    uint8_t *data;
    size_t   data_size;
    uint8_t  file_count;
    size_t   data_start;  /* offset where file data begins */
} scl_pd_t;

static bool scl_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < SCL_MAGIC_LEN + 1) return false;
    if (memcmp(data, SCL_MAGIC, SCL_MAGIC_LEN) == 0) {
        *confidence = 96;
        return true;
    }
    return false;
}

static uft_error_t scl_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t raw_size = 0;
    uint8_t *raw = uft_read_file(path, &raw_size);
    if (!raw || raw_size < SCL_MAGIC_LEN + 1) { free(raw); return UFT_ERROR_FORMAT_INVALID; }
    if (memcmp(raw, SCL_MAGIC, SCL_MAGIC_LEN) != 0) { free(raw); return UFT_ERROR_FORMAT_INVALID; }

    scl_pd_t *p = calloc(1, sizeof(scl_pd_t));
    if (!p) { free(raw); return UFT_ERROR_NO_MEMORY; }
    p->data = raw;
    p->data_size = raw_size;
    p->file_count = raw[SCL_MAGIC_LEN];
    p->data_start = SCL_MAGIC_LEN + 1 + (size_t)p->file_count * SCL_ENTRY_SIZE;

    /* Geometry: treat as single-track container with N virtual sectors */
    uint32_t total_data = (raw_size > p->data_start) ? (uint32_t)(raw_size - p->data_start) : 0;
    uint32_t total_secs = (total_data + SCL_SECTOR_SIZE - 1) / SCL_SECTOR_SIZE;

    disk->plugin_data = p;
    disk->geometry.cylinders = (total_secs + 15) / 16;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 16;
    disk->geometry.sector_size = SCL_SECTOR_SIZE;
    disk->geometry.total_sectors = total_secs;
    return UFT_OK;
}

static void scl_close(uft_disk_t *disk) {
    scl_pd_t *p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t scl_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    scl_pd_t *p = disk->plugin_data;
    if (!p || !p->data || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    for (int s = 0; s < 16; s++) {
        uint32_t sec_idx = (uint32_t)cyl * 16 + s;
        size_t off = p->data_start + (size_t)sec_idx * SCL_SECTOR_SIZE;
        if (off + SCL_SECTOR_SIZE > p->data_size) break;
        uft_format_add_sector(track, (uint8_t)s, p->data + off,
                              SCL_SECTOR_SIZE, (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

/* Write track: modifies in-memory SCL data buffer directly.
 * Sector data lives at data_start + sec_idx * 256 in the raw buffer. */
static uft_error_t scl_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
    scl_pd_t *p = disk->plugin_data;
    if (!p || !p->data || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    for (size_t s = 0; s < track->sector_count && (int)s < 16; s++) {
        uint32_t sec_idx = (uint32_t)cyl * 16 + (uint32_t)s;
        size_t off = p->data_start + (size_t)sec_idx * SCL_SECTOR_SIZE;
        if (off + SCL_SECTOR_SIZE > p->data_size) break;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[SCL_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, SCL_SECTOR_SIZE); data = pad;
        }
        memcpy(p->data + off, data, SCL_SECTOR_SIZE);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_scl = {
    .name = "SCL", .description = "ZX Spectrum SCL Container",
    .extensions = "scl", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = scl_plugin_probe, .open = scl_open,
    .close = scl_close, .read_track = scl_read_track,
    .write_track = scl_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(scl)
