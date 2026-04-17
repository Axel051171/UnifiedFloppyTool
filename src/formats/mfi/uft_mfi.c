/**
 * @file uft_mfi.c
 * @brief MAME Floppy Image (MFI) Plugin
 *
 * MFI is MAME's native floppy disk format designed for cycle-accurate
 * floppy emulation. It stores flux-level timing data per track.
 *
 * File layout:
 *   Offset  Size  Description
 *   0x00    8     Magic "MAMEFLOP"
 *   0x08    4     Form factor (LE32): 1=3.5", 2=5.25", 3=8"
 *   0x0C    4     Reserved (0)
 *   0x10    N*16  Track entries (cylinder, head, type, offset, size, write_splice)
 *
 * Track entry (16 bytes):
 *   0x00    4     Offset in file (LE32)
 *   0x04    4     Compressed size (LE32)
 *   0x08    4     Uncompressed size (LE32)
 *   0x0C    4     Write splice position (LE32)
 *
 * Track data is zlib-compressed. Each flux cell is 4 bytes (LE32):
 *   Bits 0-27:  Cell size in attoseconds / 2^32 (relative to 200ms rotation)
 *   Bits 28-31: Cell type (0=normal, 1=weak)
 *
 * Since MFI stores flux data, we present it as raw track data.
 * Full flux decoding requires PLL — this plugin provides the container.
 *
 * Reference: MAME source (lib/formats/mfi_dsk.cpp)
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MFI_MAGIC           "MAMEFLOP"
#define MFI_MAGIC_LEN       8
#define MFI_HEADER_SIZE     16
#define MFI_TRACK_ENTRY     16
#define MFI_MAX_TRACKS      168     /* 84 cyl * 2 heads */

/* Form factors */
#define MFI_FF_35           1       /* 3.5" */
#define MFI_FF_525          2       /* 5.25" */
#define MFI_FF_8            3       /* 8" */

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    uint32_t    offset;
    uint32_t    compressed_size;
    uint32_t    uncompressed_size;
    uint32_t    write_splice;
} mfi_track_entry_t;

typedef struct {
    FILE*               file;
    uint32_t            form_factor;
    uint8_t             track_count;    /* actual tracks found */
    uint8_t             cylinders;
    uint8_t             heads;
    mfi_track_entry_t   tracks[MFI_MAX_TRACKS];
} mfi_data_t;

/* ============================================================================
 * probe
 * ============================================================================ */

bool mfi_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)file_size;
    if (size < MFI_HEADER_SIZE) return false;

    if (memcmp(data, MFI_MAGIC, MFI_MAGIC_LEN) != 0)
        return false;

    uint32_t ff = uft_read_le32(data + 8);
    if (ff >= MFI_FF_35 && ff <= MFI_FF_8) {
        *confidence = 95;
    } else {
        *confidence = 70;
    }
    return true;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t mfi_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    (void)read_only;
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;

    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    size_t file_size = (size_t)fs;
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    /* Read header */
    uint8_t hdr[MFI_HEADER_SIZE];
    if (fread(hdr, 1, MFI_HEADER_SIZE, f) != MFI_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    if (memcmp(hdr, MFI_MAGIC, MFI_MAGIC_LEN) != 0) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    mfi_data_t *pdata = calloc(1, sizeof(mfi_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;
    pdata->form_factor = uft_read_le32(hdr + 8);

    /* Read track entries until EOF or max */
    size_t remaining = file_size - MFI_HEADER_SIZE;
    uint8_t count = 0;
    uint8_t max_cyl = 0;
    uint8_t max_head = 0;

    while (remaining >= MFI_TRACK_ENTRY && count < MFI_MAX_TRACKS) {
        uint8_t entry[MFI_TRACK_ENTRY];
        if (fread(entry, 1, MFI_TRACK_ENTRY, f) != MFI_TRACK_ENTRY)
            break;

        pdata->tracks[count].offset = uft_read_le32(entry);
        pdata->tracks[count].compressed_size = uft_read_le32(entry + 4);
        pdata->tracks[count].uncompressed_size = uft_read_le32(entry + 8);
        pdata->tracks[count].write_splice = uft_read_le32(entry + 12);

        /* End marker: offset 0 and size 0 */
        if (pdata->tracks[count].offset == 0 &&
            pdata->tracks[count].compressed_size == 0)
            break;

        /* Track index → cyl/head */
        uint8_t c = count / 2;
        uint8_t h = count % 2;
        if (c > max_cyl) max_cyl = c;
        if (h > max_head) max_head = h;

        count++;
        remaining -= MFI_TRACK_ENTRY;
    }

    pdata->track_count = count;
    pdata->cylinders = max_cyl + 1;
    pdata->heads = max_head + 1;

    if (count == 0) {
        free(pdata);
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = pdata->cylinders;
    disk->geometry.heads = pdata->heads;
    disk->geometry.sectors = 1;     /* flux = 1 raw track */
    disk->geometry.sector_size = 0; /* variable */
    disk->geometry.total_sectors = count;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void mfi_close(uft_disk_t *disk)
{
    mfi_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track — reads compressed flux data as raw sector
 * ============================================================================ */

static uft_error_t mfi_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    mfi_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;

    int idx = cyl * pdata->heads + head;
    if (idx >= pdata->track_count) return UFT_ERROR_INVALID_STATE;

    mfi_track_entry_t *te = &pdata->tracks[idx];
    if (te->compressed_size == 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Read compressed track data */
    if (fseek(pdata->file, (long)te->offset, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    uint8_t *buf = malloc(te->compressed_size);
    if (!buf) return UFT_ERROR_NO_MEMORY;

    if (fread(buf, 1, te->compressed_size, pdata->file) !=
        te->compressed_size) {
        free(buf);
        return UFT_ERROR_IO;
    }

    /* Store as single raw sector (compressed flux data)
     * Full decompression would require zlib — deferred to analysis layer */
    uint16_t chunk = (te->compressed_size > 65535) ?
                     65535 : (uint16_t)te->compressed_size;
    uft_format_add_sector(track, 0, buf, chunk, (uint8_t)cyl, (uint8_t)head);
    free(buf);

    return UFT_OK;
}

/* ============================================================================
 * write_track — not implemented (MFI stores compressed flux-cell streams
 * per track; writing requires MFM encoder + zlib deflate + MAME-specific
 * cell format regeneration).
 * ============================================================================ */

static uft_error_t mfi_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    (void)disk; (void)cyl; (void)head; (void)track;
    return UFT_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_mfi = {
    .name         = "MFI",
    .description  = "MAME Floppy Image",
    .extensions   = "mfi",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | UFT_FORMAT_CAP_VERIFY,
    .probe        = mfi_probe,
    .open         = mfi_open,
    .close        = mfi_close,
    .read_track   = mfi_read_track,
    .write_track  = mfi_write_track,
    .verify_track = uft_flux_verify_track,
};

UFT_REGISTER_FORMAT_PLUGIN(mfi)
