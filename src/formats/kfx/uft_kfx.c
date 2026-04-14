/**
 * @file uft_kfx.c
 * @brief KryoFlux Stream Format Plugin
 *
 * Plugin-B wrapper around the existing kf_stream_parse() API in
 * uft_kfstream_air.c. KryoFlux stream files contain raw flux timing
 * data captured at ~24MHz (SCK clock).
 *
 * Each stream file represents one track (one revolution or more).
 * Track number is encoded in the filename (trackNN.S.raw).
 *
 * Reference: KryoFlux Stream Protocol, DTC documentation
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Forward declarations from uft_kfstream_air.c
 * ============================================================================ */

/* Status codes */
typedef enum {
    KFX_OK = 0,
    KFX_ERROR = -1
} kfx_status_t;

/* Stream data (simplified — full struct in uft_kfstream_air.c) */
typedef struct {
    double   sck_value;
    double   ick_value;
    double  *flux_values;       /* Flux intervals in seconds */
    size_t  *flux_stream_pos;
    size_t   flux_count;
    size_t   index_count;
    bool     valid;
    int      status;
    /* ... more fields exist but we only need flux_values/count */
} kf_stream_minimal_t;

/* These are defined in uft_kfstream_air.c */
extern int  kf_stream_parse(const uint8_t *data, size_t size, void *stream);
extern void kf_stream_free(void *stream);

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    uint8_t    *file_data;
    size_t      file_size;
} kfx_data_t;

/* ============================================================================
 * probe — look for KryoFlux OOB marker (0x0D) in first 512 bytes
 * ============================================================================ */

bool kfx_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)file_size;
    if (size < 16) return false;

    /* KryoFlux streams start with flux data or OOB blocks.
     * OOB marker 0x0D appears early in valid streams.
     * Also check for common Flux2 headers (0x00-0x07). */
    size_t scan = (size < 512) ? size : 512;
    int oob_count = 0;

    for (size_t i = 0; i < scan; i++) {
        if (data[i] == 0x0D) oob_count++;
    }

    if (oob_count >= 2) {
        *confidence = 80;
        return true;
    }

    /* Weaker: file extension would help, but probe only sees data */
    if (oob_count >= 1) {
        *confidence = 40;
        return true;
    }

    return false;
}

/* ============================================================================
 * open — read entire file into memory
 * ============================================================================ */

static uft_error_t kfx_open(uft_disk_t *disk, const char *path,
                              bool read_only)
{
    (void)read_only;

    size_t file_size = 0;
    uint8_t *file_data = uft_read_file(path, &file_size);
    if (!file_data) return UFT_ERROR_FILE_OPEN;

    if (file_size < 16) {
        free(file_data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    kfx_data_t *pdata = calloc(1, sizeof(kfx_data_t));
    if (!pdata) { free(file_data); return UFT_ERROR_NO_MEMORY; }

    pdata->file_data = file_data;
    pdata->file_size = file_size;

    disk->plugin_data = pdata;
    /* KryoFlux: each file = 1 track, geometry unknown from single file */
    disk->geometry.cylinders = 1;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 1;
    disk->geometry.sector_size = 0;  /* flux = variable */
    disk->geometry.total_sectors = 1;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void kfx_close(uft_disk_t *disk)
{
    kfx_data_t *pdata = disk->plugin_data;
    if (pdata) {
        free(pdata->file_data);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track — parse flux stream and return raw data
 * ============================================================================ */

static uft_error_t kfx_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    kfx_data_t *pdata = disk->plugin_data;
    if (!pdata || cyl != 0 || head != 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Store raw stream data as a single sector for now.
     * Full flux parsing would use kf_stream_parse() to extract
     * flux_values[], but that requires the full kf_stream_t struct
     * which is internal to uft_kfstream_air.c. */
    uint16_t chunk = (pdata->file_size > 65535) ?
                     65535 : (uint16_t)pdata->file_size;
    uft_format_add_sector(track, 0, pdata->file_data, chunk, 0, 0);

    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_kfx = {
    .name         = "KFX",
    .description  = "KryoFlux Stream Format",
    .extensions   = "raw",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX,
    .probe        = kfx_probe,
    .open         = kfx_open,
    .close        = kfx_close,
    .read_track   = kfx_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(kfx)
