/**
 * @file uft_st.c
 * @brief Atari ST Raw Disk Image Plugin
 *
 * ST images are headerless raw sector dumps of Atari ST floppy disks.
 * Geometry is determined entirely from file size:
 *
 *   360K  = 80 cyl x 1 head x  9 spt x 512 = 368,640
 *   400K  = 80 cyl x 1 head x 10 spt x 512 = 409,600
 *   720K  = 80 cyl x 2 head x  9 spt x 512 = 737,280
 *   800K  = 80 cyl x 2 head x 10 spt x 512 = 819,200
 *   1.44M = 80 cyl x 2 head x 18 spt x 512 = 1,474,560
 *   360K  = 40 cyl x 2 head x  9 spt x 512 = 368,640  (alt, rare)
 *
 * The boot sector (sector 0) may contain a BPB with geometry hints,
 * but many ST images lack a valid BPB. File size is authoritative.
 *
 * Reference: Atari ST Developer Documentation, TOS BPB layout
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ST_SECTOR_SIZE      512
#define ST_MAX_CYLINDERS    85
#define ST_MAX_HEADS        2
#define ST_MAX_SPT          18

/* Known ST image sizes */
#define ST_SIZE_SS_DD_9     368640u     /* 80x1x9x512 */
#define ST_SIZE_SS_DD_10    409600u     /* 80x1x10x512 */
#define ST_SIZE_DS_DD_9     737280u     /* 80x2x9x512 */
#define ST_SIZE_DS_DD_10    819200u     /* 80x2x10x512 */
#define ST_SIZE_DS_HD       1474560u    /* 80x2x18x512 */
#define ST_SIZE_DS_ED       2949120u    /* 80x2x36x512 (rare) */

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    FILE*       file;
    uint8_t     cylinders;
    uint8_t     heads;
    uint8_t     sectors_per_track;
} st_data_t;

/* ============================================================================
 * Geometry detection from file size
 * ============================================================================ */

static bool st_detect_geometry(size_t file_size,
                               uint8_t *cyl, uint8_t *heads, uint8_t *spt)
{
    /* Check all known ST sizes */
    switch (file_size) {
        case ST_SIZE_SS_DD_9:
            *cyl = 80; *heads = 1; *spt = 9;
            return true;
        case ST_SIZE_SS_DD_10:
            *cyl = 80; *heads = 1; *spt = 10;
            return true;
        case ST_SIZE_DS_DD_9:
            *cyl = 80; *heads = 2; *spt = 9;
            return true;
        case ST_SIZE_DS_DD_10:
            *cyl = 80; *heads = 2; *spt = 10;
            return true;
        case ST_SIZE_DS_HD:
            *cyl = 80; *heads = 2; *spt = 18;
            return true;
        case ST_SIZE_DS_ED:
            *cyl = 80; *heads = 2; *spt = 36;
            return true;
        default:
            break;
    }

    /* Try to infer geometry: must be divisible by 512 */
    if (file_size == 0 || file_size % ST_SECTOR_SIZE != 0)
        return false;

    uint32_t total_sectors = (uint32_t)(file_size / ST_SECTOR_SIZE);

    /* Try common geometries: 80 cyl, 2 heads */
    for (uint8_t s = 9; s <= ST_MAX_SPT; s++) {
        if (total_sectors == 80u * 2 * s) {
            *cyl = 80; *heads = 2; *spt = s;
            return true;
        }
    }
    /* Try 80 cyl, 1 head */
    for (uint8_t s = 9; s <= ST_MAX_SPT; s++) {
        if (total_sectors == 80u * 1 * s) {
            *cyl = 80; *heads = 1; *spt = s;
            return true;
        }
    }
    /* Try 40 cyl (rare) */
    if (total_sectors == 40u * 2 * 9) {
        *cyl = 40; *heads = 2; *spt = 9;
        return true;
    }
    if (total_sectors == 40u * 1 * 9) {
        *cyl = 40; *heads = 1; *spt = 9;
        return true;
    }

    return false;
}

/* ============================================================================
 * probe — fast, no malloc, no fopen
 * ============================================================================ */

bool st_probe(const uint8_t *data, size_t size, size_t file_size,
              int *confidence)
{
    uint8_t cyl, heads, spt;

    /* Must match a known geometry */
    if (!st_detect_geometry(file_size, &cyl, &heads, &spt))
        return false;

    /* If we have at least the boot sector, check for Atari hints */
    if (size >= 512) {
        /* Atari ST boot: 68000 BRA.S instruction (0x60 xx) */
        if (data[0] == 0x60) {
            *confidence = 80;
            return true;
        }
        /* x86 JMP (sometimes used in non-Atari raw images) */
        if (data[0] == 0xEB && data[2] == 0x90) {
            *confidence = 40;  /* Could be PC IMG */
            return true;
        }
        /* Check BPB: bytes_per_sector == 512, valid sectors_per_track */
        uint16_t bps = data[11] | ((uint16_t)data[12] << 8);
        uint16_t bpb_spt = data[24] | ((uint16_t)data[25] << 8);
        if (bps == 512 && bpb_spt >= 9 && bpb_spt <= 18) {
            *confidence = 60;
            return true;
        }
    }

    /* Size alone is a weak match (could be IMG/IMA) */
    *confidence = 30;
    return true;
}

/* ============================================================================
 * open — determine geometry, keep file handle
 * ============================================================================ */

static uft_error_t st_open(uft_disk_t *disk, const char *path,
                            bool read_only)
{
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long ftell_result = ftell(f);
    if (ftell_result < 0) { fclose(f); return UFT_ERROR_IO; }
    size_t file_size = (size_t)ftell_result;
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    /* Detect geometry */
    uint8_t cyl, heads, spt;
    if (!st_detect_geometry(file_size, &cyl, &heads, &spt)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* Allocate plugin data */
    st_data_t *pdata = calloc(1, sizeof(st_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;
    pdata->cylinders = cyl;
    pdata->heads = heads;
    pdata->sectors_per_track = spt;

    /* Fill disk geometry */
    disk->plugin_data = pdata;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = ST_SECTOR_SIZE;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * spt;

    return UFT_OK;
}

/* ============================================================================
 * close — release resources
 * ============================================================================ */

static void st_close(uft_disk_t *disk)
{
    st_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track — direct offset calculation, no header
 * ============================================================================ */

static uft_error_t st_read_track(uft_disk_t *disk, int cyl, int head,
                                  uft_track_t *track)
{
    st_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (cyl >= pdata->cylinders || head >= pdata->heads)
        return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Track offset: (cyl * heads + head) * spt * sector_size */
    uint32_t track_index = (uint32_t)cyl * pdata->heads + (uint32_t)head;
    long track_offset = (long)(track_index * pdata->sectors_per_track
                               * ST_SECTOR_SIZE);

    if (fseek(pdata->file, track_offset, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    uint8_t buf[ST_SECTOR_SIZE];

    for (int s = 0; s < pdata->sectors_per_track; s++) {
        if (fread(buf, 1, ST_SECTOR_SIZE, pdata->file) != ST_SECTOR_SIZE)
            return UFT_ERROR_IO;

        uft_format_add_sector(track, (uint8_t)s, buf,
                              ST_SECTOR_SIZE, (uint8_t)cyl, (uint8_t)head);
    }

    return UFT_OK;
}

/* ============================================================================
 * write_track — write back sector data
 * ============================================================================ */

static uft_error_t st_write_track(uft_disk_t *disk, int cyl, int head,
                                   const uft_track_t *track)
{
    st_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    if (cyl >= pdata->cylinders || head >= pdata->heads)
        return UFT_ERROR_INVALID_STATE;

    uint32_t track_index = (uint32_t)cyl * pdata->heads + (uint32_t)head;
    long track_offset = (long)(track_index * pdata->sectors_per_track
                               * ST_SECTOR_SIZE);

    if (fseek(pdata->file, track_offset, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    for (size_t s = 0; s < track->sector_count && s < pdata->sectors_per_track; s++) {
        const uft_sector_t *sec = &track->sectors[s];
        const uint8_t *data = sec->data;
        size_t len = sec->data_len;

        if (!data || len == 0) {
            /* Write zeros for missing sectors */
            uint8_t zeros[ST_SECTOR_SIZE];
            memset(zeros, 0, ST_SECTOR_SIZE);
            if (fwrite(zeros, 1, ST_SECTOR_SIZE, pdata->file) != ST_SECTOR_SIZE)
                return UFT_ERROR_IO;
        } else {
            /* Pad/truncate to sector size */
            uint8_t buf[ST_SECTOR_SIZE];
            memset(buf, 0, ST_SECTOR_SIZE);
            memcpy(buf, data, len < ST_SECTOR_SIZE ? len : ST_SECTOR_SIZE);
            if (fwrite(buf, 1, ST_SECTOR_SIZE, pdata->file) != ST_SECTOR_SIZE)
                return UFT_ERROR_IO;
        }
    }

    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_st = {
    .name         = "ST",
    .description  = "Atari ST Raw Disk Image",
    .extensions   = "st",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe        = st_probe,
    .open         = st_open,
    .close        = st_close,
    .read_track   = st_read_track,
    .write_track  = st_write_track,
};

UFT_REGISTER_FORMAT_PLUGIN(st)
