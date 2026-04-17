/**
 * @file uft_cas.c
 * @brief MSX CAS Cassette Image Plugin
 *
 * CAS is the standard cassette tape image format for MSX computers.
 * Files consist of header blocks (8-byte signature) followed by data.
 *
 * MSX CAS header signature: 1F A6 DE BA CC 13 7D 74
 *
 * Block types (identified by 10-byte header after CAS sig):
 *   - Binary:   header D0 D0 D0 D0 D0 D0 D0 D0 D0 D0
 *   - BASIC:    header D3 D3 D3 D3 D3 D3 D3 D3 D3 D3
 *   - ASCII:    header EA EA EA EA EA EA EA EA EA EA
 *   - Custom:   header varies
 *
 * Since CAS is a tape format (not disk), we represent the data as a
 * single-track image with one virtual sector per CAS block.
 *
 * Reference: MSX Resource Center wiki, openMSX source
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CAS_HEADER_SIZE     8
#define CAS_MAX_BLOCKS      256
#define CAS_MAX_BLOCK_SIZE  65536

static const uint8_t CAS_MSX_MAGIC[8] = {
    0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74
};

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    uint8_t*    data;           /* entire file in memory */
    size_t      data_size;
    uint32_t    block_offsets[CAS_MAX_BLOCKS]; /* offset of each block's data */
    uint32_t    block_sizes[CAS_MAX_BLOCKS];
    uint16_t    block_count;
} cas_data_t;

/* ============================================================================
 * Block scanner — find all CAS header signatures
 * ============================================================================ */

static uint16_t cas_scan_blocks(const uint8_t *data, size_t size,
                                uint32_t *offsets, uint32_t *sizes,
                                uint16_t max_blocks)
{
    uint16_t count = 0;
    size_t pos = 0;

    while (pos + CAS_HEADER_SIZE <= size && count < max_blocks) {
        if (memcmp(data + pos, CAS_MSX_MAGIC, CAS_HEADER_SIZE) == 0) {
            size_t block_start = pos + CAS_HEADER_SIZE;

            /* Find next header or end of file */
            size_t next = block_start;
            while (next + CAS_HEADER_SIZE <= size) {
                if (memcmp(data + next, CAS_MSX_MAGIC, CAS_HEADER_SIZE) == 0)
                    break;
                next++;
            }
            if (next + CAS_HEADER_SIZE > size) next = size;

            offsets[count] = (uint32_t)block_start;
            sizes[count] = (uint32_t)(next - block_start);
            count++;
            pos = next;
        } else {
            pos++;
        }
    }

    return count;
}

/* ============================================================================
 * probe
 * ============================================================================ */

bool cas_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)file_size;
    if (size < CAS_HEADER_SIZE) return false;

    if (memcmp(data, CAS_MSX_MAGIC, CAS_HEADER_SIZE) == 0) {
        *confidence = 95;
        return true;
    }

    return false;
}

/* ============================================================================
 * open — read entire file, scan blocks
 * ============================================================================ */

static uft_error_t cas_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    (void)read_only;

    size_t file_size = 0;
    uint8_t *file_data = uft_read_file(path, &file_size);
    if (!file_data) return UFT_ERROR_FILE_OPEN;

    if (file_size < CAS_HEADER_SIZE ||
        memcmp(file_data, CAS_MSX_MAGIC, CAS_HEADER_SIZE) != 0) {
        free(file_data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    cas_data_t *pdata = calloc(1, sizeof(cas_data_t));
    if (!pdata) { free(file_data); return UFT_ERROR_NO_MEMORY; }

    pdata->data = file_data;
    pdata->data_size = file_size;
    pdata->block_count = cas_scan_blocks(file_data, file_size,
                                          pdata->block_offsets,
                                          pdata->block_sizes,
                                          CAS_MAX_BLOCKS);

    if (pdata->block_count == 0) {
        free(file_data);
        free(pdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = 1;
    disk->geometry.heads = 1;
    disk->geometry.sectors = pdata->block_count;
    disk->geometry.sector_size = 512; /* virtual */
    disk->geometry.total_sectors = pdata->block_count;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void cas_close(uft_disk_t *disk)
{
    cas_data_t *pdata = disk->plugin_data;
    if (pdata) {
        free(pdata->data);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track — one track, each sector = one CAS block
 * ============================================================================ */

static uft_error_t cas_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    cas_data_t *pdata = disk->plugin_data;
    if (!pdata || cyl != 0 || head != 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, 0, 0);

    for (int s = 0; s < pdata->block_count; s++) {
        uint32_t off = pdata->block_offsets[s];
        uint32_t sz = pdata->block_sizes[s];

        if (off + sz > pdata->data_size) break;
        if (sz > CAS_MAX_BLOCK_SIZE) sz = CAS_MAX_BLOCK_SIZE;

        uft_format_add_sector(track, (uint8_t)s,
                              pdata->data + off,
                              (uint16_t)(sz > 65535 ? 65535 : sz),
                              0, 0);
    }

    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * NOTE: write_track omitted by design — CAS is a linear tape (cassette)
 * image, not a floppy. It has no track/sector addressing, so writing a
 * uft_track_t here has no meaningful mapping.
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_cas = {
    .name         = "CAS",
    .description  = "MSX Cassette Tape Image",
    .extensions   = "cas",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe        = cas_probe,
    .open         = cas_open,
    .close        = cas_close,
    .read_track   = cas_read_track,
    .verify_track = uft_generic_verify_track,
};

UFT_REGISTER_FORMAT_PLUGIN(cas)
