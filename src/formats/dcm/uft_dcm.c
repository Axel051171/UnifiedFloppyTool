/**
 * @file uft_dcm.c
 * @brief DCM (Disk COMpression) Format Plugin — full decompression
 *
 * DCM is a sector compression format for Atari 8-bit floppy disks.
 * Developed 1988 by Bob Puff (DiskCommunicator) for BBS distribution.
 *
 * Supports:
 *   - Single Density (SD: 90K, 720 sectors x 128 byte)
 *   - Enhanced Density (ED: 130K, 1040 sectors x 128 byte)
 *   - Double Density (DD: 180K, 720 sectors x 256 byte)
 *   - Quad Density (QD: 360K, 1440 sectors x 256 byte)
 *   - Multi-pass archives
 *   - All 5 compression types (Modify/Same/Compress/Change/Gap)
 *
 * CRITICAL: Sectors 1-3 are ALWAYS 128 bytes, even on DD disks.
 *
 * Reference: Bob Puff DiskComm source, Atari800 DCM loader
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DCM_MAGIC_FA        0xFA    /* Single Density */
#define DCM_MAGIC_F9        0xF9    /* Enhanced/Double Density */
#define DCM_MAGIC_F8        0xF8    /* Double Density (alt) */

#define ATR_BOOT_SECTORS    3
#define ATR_BOOT_SS         128

/* Compression type codes */
#define DCM_CMD_MODIFY      0x41    /* 'A' — raw sector */
#define DCM_CMD_SAME        0x42    /* 'B' — same as previous */
#define DCM_CMD_COMPRESS    0x43    /* 'C' — RLE */
#define DCM_CMD_CHANGE      0x44    /* 'D' — delta from previous */
#define DCM_CMD_GAP         0x45    /* 'E' — zeroes */
#define DCM_CMD_END         0x46    /* 'F' — end of pass */

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    uint8_t*    disk_data;
    uint32_t    disk_size;
    uint16_t    sector_size;
    uint32_t    total_sectors;
    uint8_t     density;
} dcm_plugin_data_t;

/* ============================================================================
 * Sector offset helpers (boot sector exception)
 * ============================================================================ */

static size_t dcm_sector_offset(uint32_t sec, uint16_t ss) {
    if (sec < 1) return 0;
    if (sec <= ATR_BOOT_SECTORS)
        return (size_t)(sec - 1) * ATR_BOOT_SS;
    return (size_t)ATR_BOOT_SECTORS * ATR_BOOT_SS
         + (size_t)(sec - ATR_BOOT_SECTORS - 1) * ss;
}

static uint16_t dcm_sec_size(uint32_t sec, uint16_t ss) {
    return (sec <= ATR_BOOT_SECTORS) ? ATR_BOOT_SS : ss;
}

/* ============================================================================
 * Decompression engine
 * ============================================================================ */

static uft_error_t dcm_decompress(const uint8_t *data, size_t data_size,
                                   uint8_t *out, uint32_t out_size,
                                   uint16_t ss, uint32_t total)
{
    if (!data || !out) return UFT_ERROR_INVALID_STATE;

    size_t pos = 0;
    uint8_t prev[256];
    memset(prev, 0, sizeof(prev));
    bool last_pass = false;

    while (pos < data_size && !last_pass) {
        /* Pass header: magic(1) + flags(1) + start_sector(2) */
        if (pos + 4 > data_size) break;

        uint8_t magic = data[pos++];
        if (magic != DCM_MAGIC_FA && magic != DCM_MAGIC_F9 && magic != DCM_MAGIC_F8)
            break;

        uint8_t flags = data[pos++];
        last_pass = (flags & 0x40) != 0;

        uint16_t cur = (uint16_t)(data[pos] | (data[pos+1] << 8));
        pos += 2;
        if (cur < 1 || cur > total) return UFT_ERROR_FORMAT_INVALID;

        /* Sector loop */
        for (;;) {
            if (pos >= data_size) break;

            uint8_t cb = data[pos++];
            uint8_t cmd = cb & 0x7F;

            /* Explicit sector number */
            if (cb & 0x80) {
                if (pos + 2 > data_size) return UFT_ERROR_IO;
                cur = (uint16_t)(data[pos] | (data[pos+1] << 8));
                pos += 2;
                if (cur < 1 || cur > total) return UFT_ERROR_FORMAT_INVALID;
            }

            if (cmd == DCM_CMD_END) break;

            size_t off = dcm_sector_offset(cur, ss);
            uint16_t sz = dcm_sec_size(cur, ss);
            if (off + sz > out_size) return UFT_ERROR_FORMAT_INVALID;
            uint8_t *sec = out + off;

            switch (cmd) {
                case DCM_CMD_MODIFY:
                    if (pos + sz > data_size) return UFT_ERROR_IO;
                    memcpy(sec, data + pos, sz);
                    pos += sz;
                    memcpy(prev, sec, sz);
                    break;

                case DCM_CMD_SAME:
                    memcpy(sec, prev, sz);
                    break;

                case DCM_CMD_COMPRESS: {
                    uint16_t op = 0;
                    while (op < sz) {
                        if (pos >= data_size) return UFT_ERROR_IO;
                        uint8_t esc = data[pos++];
                        if (esc == 0) {
                            memset(sec + op, 0, sz - op);
                            op = sz;
                        } else {
                            if (pos >= data_size) return UFT_ERROR_IO;
                            uint8_t end = data[pos++];
                            uint16_t run = (end >= esc) ? (uint16_t)(end - esc + 1)
                                                        : (uint16_t)(256 - esc + end + 1);
                            if (op + run > sz) return UFT_ERROR_FORMAT_INVALID;
                            if (pos >= data_size) return UFT_ERROR_IO;
                            uint8_t fill = data[pos++];
                            memset(sec + op, fill, run);
                            op += run;
                        }
                    }
                    memcpy(prev, sec, sz);
                    break;
                }

                case DCM_CMD_CHANGE:
                    memcpy(sec, prev, sz);
                    for (;;) {
                        if (pos >= data_size) return UFT_ERROR_IO;
                        uint8_t ob = data[pos++];
                        if (ob == 0) break;
                        if (pos >= data_size) return UFT_ERROR_IO;
                        uint8_t eb = data[pos++];
                        uint16_t start = ob - 1;
                        uint16_t end = (eb == 0) ? sz : (uint16_t)eb;
                        if (start >= sz || end > sz || start >= end)
                            return UFT_ERROR_FORMAT_INVALID;
                        if (pos + (end - start) > data_size) return UFT_ERROR_IO;
                        memcpy(sec + start, data + pos, end - start);
                        pos += (end - start);
                    }
                    memcpy(prev, sec, sz);
                    break;

                case DCM_CMD_GAP:
                    memset(sec, 0, sz);
                    memset(prev, 0, sz);
                    break;

                default:
                    return UFT_ERROR_FORMAT_INVALID;
            }

            if (!(cb & 0x80)) {
                cur++;
                if (cur > total) break;
            }
        }
    }
    return UFT_OK;
}

/* ============================================================================
 * probe
 * ============================================================================ */

static bool uft_dcm_plugin_probe(const uint8_t *data, size_t size,
                                  size_t file_size, int *confidence)
{
    (void)file_size;
    if (size < 2) return false;
    if (data[0] == DCM_MAGIC_FA || data[0] == DCM_MAGIC_F9 || data[0] == DCM_MAGIC_F8) {
        *confidence = 90;
        return true;
    }
    return false;
}

/* ============================================================================
 * open — decompress entire DCM into memory buffer
 * ============================================================================ */

static uft_error_t dcm_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;

    size_t file_size = 0;
    uint8_t *file_data = uft_read_file(path, &file_size);
    if (!file_data || file_size < 5) { free(file_data); return UFT_ERROR_FORMAT_INVALID; }

    uint8_t magic = file_data[0];
    if (magic != DCM_MAGIC_FA && magic != DCM_MAGIC_F9 && magic != DCM_MAGIC_F8) {
        free(file_data); return UFT_ERROR_FORMAT_INVALID;
    }

    /* Density from flags byte (bits 0-4) */
    uint8_t density = file_data[1] & 0x1F;
    if (density > 3) density = 0;

    uint16_t ss;
    uint32_t total;
    switch (density) {
        case 0:  ss = 128; total = 720;  break;
        case 1:  ss = 128; total = 1040; break;
        case 2:  ss = 256; total = 720;  break;
        case 3:  ss = 256; total = 1440; break;
        default: ss = 128; total = 720;  break;
    }

    uint32_t disk_size = (uint32_t)ATR_BOOT_SECTORS * ATR_BOOT_SS
                       + (total > ATR_BOOT_SECTORS ? total - ATR_BOOT_SECTORS : 0) * ss;

    uint8_t *disk_buf = calloc(1, disk_size);
    if (!disk_buf) { free(file_data); return UFT_ERROR_NO_MEMORY; }

    uft_error_t err = dcm_decompress(file_data, file_size, disk_buf, disk_size, ss, total);
    free(file_data);

    if (err != UFT_OK) { free(disk_buf); return err; }

    dcm_plugin_data_t *p = calloc(1, sizeof(dcm_plugin_data_t));
    if (!p) { free(disk_buf); return UFT_ERROR_NO_MEMORY; }

    p->disk_data = disk_buf;
    p->disk_size = disk_size;
    p->sector_size = ss;
    p->total_sectors = total;
    p->density = density;

    disk->plugin_data = p;
    disk->geometry.cylinders = (total + 17) / 18;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = ss;
    disk->geometry.total_sectors = total;
    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void dcm_close(uft_disk_t *disk)
{
    dcm_plugin_data_t *p = disk->plugin_data;
    if (p) { free(p->disk_data); free(p); disk->plugin_data = NULL; }
}

/* ============================================================================
 * read_track
 * ============================================================================ */

static uft_error_t dcm_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    dcm_plugin_data_t *p = disk->plugin_data;
    if (!p || !p->disk_data || head != 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    for (int s = 0; s < 18; s++) {
        uint32_t sec_num = (uint32_t)(cyl * 18 + s + 1);
        if (sec_num > p->total_sectors) break;

        uint16_t sz = dcm_sec_size(sec_num, p->sector_size);
        size_t off = dcm_sector_offset(sec_num, p->sector_size);
        if (off + sz > p->disk_size) break;

        uft_format_add_sector(track, (uint8_t)s,
                              p->disk_data + off, sz,
                              (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

/* ============================================================================
 * write_track — modify decompressed in-memory buffer
 * ============================================================================ */

static uft_error_t dcm_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track)
{
    dcm_plugin_data_t *p = disk->plugin_data;
    if (!p || !p->disk_data || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    for (int s = 0; s < 18; s++) {
        uint32_t sec_num = (uint32_t)(cyl * 18 + s + 1);
        if (sec_num > p->total_sectors) break;

        uint16_t sz = dcm_sec_size(sec_num, p->sector_size);
        size_t off = dcm_sector_offset(sec_num, p->sector_size);
        if (off + sz > p->disk_size) break;

        /* Find matching sector in input track */
        for (size_t ts = 0; ts < track->sector_count; ts++) {
            if (track->sectors[ts].id.sector == (uint8_t)s) {
                const uint8_t *src = track->sectors[ts].data;
                if (src && track->sectors[ts].data_len >= sz)
                    memcpy(p->disk_data + off, src, sz);
                break;
            }
        }
    }
    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_dcm = {
    .name         = "DCM",
    .description  = "Atari 8-bit Compressed Disk (DiskCOMm)",
    .extensions   = "dcm",
    .version      = 0x00020000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe        = uft_dcm_plugin_probe,
    .open         = dcm_open,
    .close        = dcm_close,
    .read_track   = dcm_read_track,
    .write_track  = dcm_write_track,
    .verify_track = uft_generic_verify_track,
};

UFT_REGISTER_FORMAT_PLUGIN(dcm)
