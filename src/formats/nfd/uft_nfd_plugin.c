/**
 * @file uft_nfd_plugin.c
 * @brief NFD (T98-Next PC-98) Plugin-B
 *
 * NFD is the disk image format for T98-Next (PC-98 emulator).
 *
 * R0 format:
 *   Header: "T98FDDIMAGE.R0\0" (15 bytes + null) at offset 0
 *   Offset 0x120: 164-entry sector index table, each 16 bytes:
 *     0: media_type
 *     1: cylinder
 *     2: head
 *     3: record_num (number of sectors in this track entry)
 *     4: sector_length code (0=128, 1=256, 2=512, 3=1024)
 *     5: MFM flag (1=MFM, 0=FM)
 *     6: DDAM flag (1=deleted data)
 *     7: status (FDC status)
 *     8: ST0 (FDC result)
 *     9: ST1
 *    10: ST2
 *    11: PDA (physical disk address)
 *    12-15: sector_data_offset (LE32, absolute file offset)
 *
 *   Sector data follows at offsets specified in the table.
 *
 * R1 format:
 *   Header: "T98FDDIMAGE.R1\0" (15 bytes + null)
 *   Extended format with additional metadata (similar structure).
 *
 * Reference: T98-Next documentation, common98 source
 */
#include "uft/uft_format_common.h"

#define NFD_SIG_R0      "T98FDDIMAGE.R0"
#define NFD_SIG_R1      "T98FDDIMAGE.R1"
#define NFD_SIG_BASE    "T98FDDIMAGE"
#define NFD_SIG_LEN     15      /* including ".R0" or ".R1" */
#define NFD_INDEX_OFF   0x120   /* sector index table offset */
#define NFD_INDEX_ENTRY 16      /* bytes per index entry */
#define NFD_INDEX_COUNT 164     /* number of entries */

typedef struct {
    uint8_t *data;
    size_t   size;
    bool     is_r1;
    uint8_t  max_cyl;
    uint8_t  max_head;
} nfd_pd_t;

static uint16_t nfd_sec_size(uint8_t code)
{
    switch (code) {
        case 0: return 128;
        case 1: return 256;
        case 2: return 512;
        case 3: return 1024;
        default: return 512;
    }
}

static bool nfd_probe(const uint8_t *data, size_t size, size_t file_size,
                       int *confidence)
{
    (void)file_size;
    if (size < NFD_SIG_LEN) return false;
    if (memcmp(data, NFD_SIG_BASE, 11) == 0) {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t nfd_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < NFD_INDEX_OFF + NFD_INDEX_ENTRY) {
        free(data);
        return UFT_ERROR_FILE_OPEN;
    }

    if (memcmp(data, NFD_SIG_BASE, 11) != 0) {
        free(data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    bool is_r1 = (memcmp(data, NFD_SIG_R1, NFD_SIG_LEN) == 0);

    /* Scan index table to determine geometry */
    uint8_t max_cyl = 0;
    uint8_t max_head = 0;
    uint8_t max_spt = 0;
    uint16_t sec_size = 512;

    for (int i = 0; i < NFD_INDEX_COUNT; i++) {
        size_t off = NFD_INDEX_OFF + (size_t)i * NFD_INDEX_ENTRY;
        if (off + NFD_INDEX_ENTRY > file_size) break;

        uint8_t cyl = data[off + 1];
        uint8_t head = data[off + 2];
        uint8_t nsec = data[off + 3];
        uint8_t scode = data[off + 4];
        uint32_t data_off = uft_read_le32(data + off + 12);

        /* Skip empty entries */
        if (nsec == 0 && data_off == 0) continue;

        if (cyl > max_cyl) max_cyl = cyl;
        if (head > max_head) max_head = head;
        if (nsec > max_spt) max_spt = nsec;
        sec_size = nfd_sec_size(scode);
    }

    if (max_cyl > 200 || max_head > 1) {
        free(data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    nfd_pd_t *p = calloc(1, sizeof(nfd_pd_t));
    if (!p) { free(data); return UFT_ERROR_NO_MEMORY; }
    p->data = data;
    p->size = file_size;
    p->is_r1 = is_r1;
    p->max_cyl = max_cyl;
    p->max_head = max_head;

    disk->plugin_data = p;
    disk->geometry.cylinders = max_cyl + 1;
    disk->geometry.heads = max_head + 1;
    disk->geometry.sectors = max_spt > 0 ? max_spt : 8;
    disk->geometry.sector_size = sec_size;
    disk->geometry.total_sectors = (uint32_t)(max_cyl + 1) *
                                   (max_head + 1) *
                                   (max_spt > 0 ? max_spt : 8);
    return UFT_OK;
}

static void nfd_close(uft_disk_t *disk)
{
    nfd_pd_t *p = disk->plugin_data;
    if (p) {
        free(p->data);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t nfd_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    nfd_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Search index table for entries matching (cyl, head) */
    for (int i = 0; i < NFD_INDEX_COUNT; i++) {
        size_t off = NFD_INDEX_OFF + (size_t)i * NFD_INDEX_ENTRY;
        if (off + NFD_INDEX_ENTRY > p->size) break;

        uint8_t e_cyl    = p->data[off + 1];
        uint8_t e_head   = p->data[off + 2];
        uint8_t e_nsec   = p->data[off + 3];
        uint8_t e_scode  = p->data[off + 4];
        /* uint8_t e_mfm    = p->data[off + 5]; */
        uint8_t e_ddam   = p->data[off + 6];
        uint8_t e_status = p->data[off + 7];
        uint8_t e_st1    = p->data[off + 9];
        uint8_t e_st2    = p->data[off + 10];
        uint32_t data_off = uft_read_le32(p->data + off + 12);

        if ((int)e_cyl != cyl || (int)e_head != head) continue;
        if (e_nsec == 0 && data_off == 0) continue;

        uint16_t ss = nfd_sec_size(e_scode);

        /* Read each sector within this track entry.
         * NFD stores sectors sequentially from data_off.
         * e_nsec indicates how many sectors in this track entry. */
        for (int s = 0; s < e_nsec; s++) {
            size_t soff = (size_t)data_off + (size_t)s * ss;
            if (soff + ss > p->size) {
                /* Beyond file: fill with forensic pattern */
                uint8_t *fill = malloc(ss);
                if (!fill) return UFT_ERROR_NO_MEMORY;
                memset(fill, 0xE5, ss);
                uft_format_add_sector(track, (uint8_t)s, fill, ss,
                                      (uint8_t)cyl, (uint8_t)head);
                free(fill);
            } else {
                uft_format_add_sector(track, (uint8_t)s, p->data + soff,
                                      ss, (uint8_t)cyl, (uint8_t)head);
            }

            /* Propagate DDAM and FDC status flags */
            if (track->sector_count > 0) {
                size_t idx = track->sector_count - 1;
                /* DDAM flag -> deleted mark */
                if (e_ddam)
                    track->sectors[idx].deleted = true;
                /* FDC status: ST1 bit 5 (0x20) = CRC error in ID
                 * ST2 bit 5 (0x20) = CRC error in data
                 * e_status nonzero typically means abnormal completion */
                if ((e_st1 & 0x20) || (e_st2 & 0x20) || (e_status != 0))
                    track->sectors[idx].crc_ok = false;
            }
        }
        break; /* Found matching track entry */
    }
    return UFT_OK;
}

static uft_error_t nfd_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track)
{
    nfd_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    /* Search index table for entries matching (cyl, head) */
    for (int i = 0; i < NFD_INDEX_COUNT; i++) {
        size_t off = NFD_INDEX_OFF + (size_t)i * NFD_INDEX_ENTRY;
        if (off + NFD_INDEX_ENTRY > p->size) break;

        uint8_t e_cyl    = p->data[off + 1];
        uint8_t e_head   = p->data[off + 2];
        uint8_t e_nsec   = p->data[off + 3];
        uint8_t e_scode  = p->data[off + 4];
        uint32_t data_off = uft_read_le32(p->data + off + 12);

        if ((int)e_cyl != cyl || (int)e_head != head) continue;
        if (e_nsec == 0 && data_off == 0) continue;

        uint16_t ss = nfd_sec_size(e_scode);

        for (int s = 0; s < e_nsec; s++) {
            size_t soff = (size_t)data_off + (size_t)s * ss;
            if (soff + ss > p->size) break;

            /* Find matching sector in input track */
            for (size_t ts = 0; ts < track->sector_count; ts++) {
                if (track->sectors[ts].id.sector == (uint8_t)s) {
                    const uint8_t *src = track->sectors[ts].data;
                    if (src && track->sectors[ts].data_len >= ss)
                        memcpy(p->data + soff, src, ss);
                    break;
                }
            }
        }
        break; /* Found matching track entry */
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_nfd = {
    .name = "NFD", .description = "T98-Next PC-98 (NFD)",
    .extensions = "nfd", .format = UFT_FORMAT_NFD,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = nfd_probe, .open = nfd_open, .close = nfd_close,
    .read_track = nfd_read_track,
    .write_track = nfd_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(nfd)
