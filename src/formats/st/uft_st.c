/**
 * @file uft_st.c
 * @brief Atari ST Raw Disk Image Plugin
 *
 * ST images are headerless raw sector dumps of Atari ST floppy disks, so the
 * geometry has to come from somewhere else. Two sources, in this order
 * (MF-462, same order as SAMdisk 4.0 src/samdisk/st.cpp:24-67):
 *
 *   1. the BPB in the boot sector, when it is self-consistent AND accounts
 *      for the file exactly
 *   2. otherwise a scan of the cylinder/head/sector combinations ST drives
 *      actually wrote
 *
 * Why the BPB comes first. File size alone does not always decide: 368,640
 * bytes is 80x1x9 and 40x2x9, and this file's own header comment used to list
 * both while the code silently picked the first. TOS writes a DOS-compatible
 * BPB at offset 0x0B of the boot sector (bytes/sector 0x0B, total sectors
 * 0x13, sectors/track 0x18, heads 0x1A — same layout as samdisk/bpb.h), which
 * answers the question when it is present. When it is absent or inconsistent
 * — plenty of ST images have no usable BPB — the size scan decides, and its
 * order is documented below.
 *
 * The scan covers 80..84 cylinders and 8..11 sectors like SAMdisk does.
 * That range is not cosmetic: extended-format ST disks with 82 or 83 tracks
 * and 10 or 11 sectors were common, and this plugin rejected every one of
 * them outright before MF-462.
 *
 * References: SAMdisk 4.0 src/samdisk/st.cpp + bpb.h (MIT, reference oracle
 * only, see src/samdisk/README.md); Atari ST Developer Documentation, TOS
 * boot sector / BPB layout.
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ST_SECTOR_SIZE      512
#define ST_MAX_CYLINDERS    85
#define ST_MAX_HEADS        2
#define ST_MAX_SPT          36

/* BPB field offsets inside the boot sector (samdisk/bpb.h) */
#define ST_BPB_BYTES_PER_SEC 0x0B  /**< LE16 */
#define ST_BPB_TOTAL_SECS    0x13  /**< LE16 */
#define ST_BPB_SEC_PER_TRACK 0x18  /**< LE16 */
#define ST_BPB_HEADS         0x1A  /**< LE16 */

/**
 * A bootable ST disk carries a checksum: the 256 big-endian words of the boot
 * sector sum to 0x1234, and TOS refuses to boot otherwise (samdisk/st.cpp:6).
 * Its presence identifies an Atari boot sector; its absence proves nothing,
 * because non-bootable disks deliberately avoid the value.
 */
#define ST_BOOT_CHECKSUM    0x1234

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
 * Geometry detection
 * ============================================================================ */

/** Sum of the boot sector's 256 big-endian words. */
static uint16_t st_boot_checksum(const uint8_t *boot)
{
    uint16_t sum = 0;
    for (size_t i = 0; i < ST_SECTOR_SIZE; i += 2)
        sum = (uint16_t)(sum + (((uint16_t)boot[i] << 8) | boot[i + 1]));
    return sum;
}

/**
 * Geometry from the boot sector's BPB. Accepted only if the numbers are
 * self-consistent AND their product is exactly the file — a BPB that
 * describes a different disk than the one in front of us is not evidence.
 */
static bool st_geometry_from_bpb(const uint8_t *boot, size_t file_size,
                                 uint8_t *cyl, uint8_t *heads, uint8_t *spt)
{
    uint16_t bps   = uft_read_le16(&boot[ST_BPB_BYTES_PER_SEC]);
    uint16_t total = uft_read_le16(&boot[ST_BPB_TOTAL_SECS]);
    uint16_t s     = uft_read_le16(&boot[ST_BPB_SEC_PER_TRACK]);
    uint16_t h     = uft_read_le16(&boot[ST_BPB_HEADS]);

    if (bps != ST_SECTOR_SIZE || s < 1 || s > ST_MAX_SPT ||
        h < 1 || h > ST_MAX_HEADS || total < 1)
        return false;

    uint32_t per_cyl = (uint32_t)s * h;
    uint32_t c       = total / per_cyl;

    if (c < 1 || c > ST_MAX_CYLINDERS)   return false;
    if (c * per_cyl != total)            return false;  /* no partial cylinder */
    if ((size_t)total * bps != file_size) return false;  /* must be this file  */

    *cyl = (uint8_t)c; *heads = (uint8_t)h; *spt = (uint8_t)s;
    return true;
}

/**
 * Geometry from the file size alone.
 *
 * The order is the whole content of this function: several combinations can
 * produce the same size, and the first hit wins. Cylinders are tried 80 first
 * (then the extended 81..84, then the rare 40-cylinder case), heads
 * double-sided before single-sided, sectors from the most common outwards.
 * That order reproduces exactly what the previous size table decided for the
 * six sizes it listed — widening the range must not silently re-interpret
 * images that already read correctly.
 */
static bool st_geometry_from_size(size_t file_size,
                                  uint8_t *cyl, uint8_t *heads, uint8_t *spt)
{
    static const uint8_t CYLS[] = { 80, 81, 82, 83, 84, 40 };
    static const uint8_t SPTS[] = { 9, 10, 11, 18, 36, 8 };

    if (file_size == 0 || file_size % ST_SECTOR_SIZE != 0)
        return false;

    uint32_t total = (uint32_t)(file_size / ST_SECTOR_SIZE);

    for (size_t ci = 0; ci < sizeof(CYLS); ci++)
        for (uint8_t h = ST_MAX_HEADS; h >= 1; h--)
            for (size_t si = 0; si < sizeof(SPTS); si++)
                if ((uint32_t)CYLS[ci] * h * SPTS[si] == total) {
                    *cyl = CYLS[ci]; *heads = h; *spt = SPTS[si];
                    return true;
                }
    return false;
}

/**
 * @param boot boot sector, or NULL when it has not been read
 */
static bool st_detect_geometry(const uint8_t *boot, size_t file_size,
                               uint8_t *cyl, uint8_t *heads, uint8_t *spt)
{
    if (boot && st_geometry_from_bpb(boot, file_size, cyl, heads, spt))
        return true;
    return st_geometry_from_size(file_size, cyl, heads, spt);
}

/* ============================================================================
 * probe — fast, no malloc, no fopen
 * ============================================================================ */

bool st_probe(const uint8_t *data, size_t size, size_t file_size,
              int *confidence)
{
    uint8_t cyl, heads, spt;
    const uint8_t *boot = (size >= ST_SECTOR_SIZE) ? data : NULL;

    /* Must match a geometry the reader would then accept */
    if (!st_detect_geometry(boot, file_size, &cyl, &heads, &spt))
        return false;

    if (boot) {
        /* The one positive identification the format offers: TOS checks this
         * word before booting a disk, so nothing else lands on it by chance. */
        if (st_boot_checksum(boot) == ST_BOOT_CHECKSUM) {
            *confidence = 90;
            return true;
        }
        /* A BPB that describes exactly this file is strong, if mute about
         * which machine wrote it. */
        if (st_geometry_from_bpb(boot, file_size, &cyl, &heads, &spt)) {
            *confidence = 75;
            return true;
        }
        /* 68000 BRA.S — an Atari boot sector starts with it, but so does one
         * byte in sixteen of an arbitrary image. */
        if (data[0] == 0x60) {
            *confidence = 65;
            return true;
        }
        /* x86 JMP: this is far more likely a PC image of the same size. */
        if (data[0] == 0xEB && data[2] == 0x90) {
            *confidence = 30;
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

    /* Read the boot sector so the BPB can be consulted first. A file too
     * short to hold one is decided by size alone. */
    uint8_t boot[ST_SECTOR_SIZE];
    bool have_boot = (file_size >= ST_SECTOR_SIZE) &&
                     (fread(boot, 1, ST_SECTOR_SIZE, f) == ST_SECTOR_SIZE);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    /* Detect geometry */
    uint8_t cyl, heads, spt;
    if (!st_detect_geometry(have_boot ? boot : NULL, file_size,
                            &cyl, &heads, &spt)) {
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
    if (cyl < 0 || head < 0 ||
        cyl >= pdata->cylinders || head >= pdata->heads)
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
    if (cyl < 0 || head < 0 ||
        cyl >= pdata->cylinders || head >= pdata->heads)
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

static const uft_plugin_feature_t uft_format_plugin_st_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_st = {
    .name         = "ST",
    .description  = "Atari ST Raw Disk Image",
    .extensions   = "st",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_ST,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe        = st_probe,
    .open         = st_open,
    .close        = st_close,
    .read_track   = st_read_track,
    .write_track  = st_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_st_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_st_features) / sizeof(uft_format_plugin_st_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(st)
