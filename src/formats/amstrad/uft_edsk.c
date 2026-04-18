/**
 * @file uft_edsk.c
 * @brief EDSK/DSK Plugin-B Wrapper (Amstrad CPC/ZX Spectrum)
 *
 * Thin wrapper around the existing 672-LOC edsk_parser_* API in
 * uft_edsk_parser.c, exposing it as a UFT format plugin.
 *
 * Supports both standard DSK ("MV - CPC") and Extended DSK ("EXTENDED").
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Forward declarations from uft_edsk_parser.c (same translation unit)
 * ============================================================================ */

/* Opaque parser context */
typedef struct edsk_parser_ctx_s edsk_parser_ctx_t;

/* Sector from parsed track */
typedef struct {
    uint8_t track;
    uint8_t side;
    uint8_t sector_id;
    uint8_t size_code;
    uint8_t  st1;
    uint8_t  st2;
    uint16_t data_size;
    uint8_t* data;
    uint8_t* weak_data;
    int      weak_copies;
} edsk_sector_info_t;

/* Parsed track */
typedef struct {
    int track_number;
    int side;
    int sector_count;
    uint8_t sector_size_code;
    uint8_t gap3_length;
    uint8_t filler_byte;
    edsk_sector_info_t sectors[64];
    int good_sectors;
    int bad_sectors;
    int weak_sectors;
    int deleted_sectors;
    float quality_percent;
} edsk_track_info_t;

extern edsk_parser_ctx_t* edsk_parser_open(const char* path);
extern void edsk_parser_close(edsk_parser_ctx_t** ctx);
extern int  edsk_parser_read_track(edsk_parser_ctx_t* ctx,
                                    int track_num, int side,
                                    edsk_track_info_t** track_out);
extern void edsk_parser_free_track(edsk_track_info_t** track);
extern void edsk_parser_get_info(edsk_parser_ctx_t* ctx,
                                  int* num_tracks, int* num_sides,
                                  bool* is_extended,
                                  char* creator, size_t creator_size);

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    edsk_parser_ctx_t* ctx;
} edsk_plugin_data_t;

/* ============================================================================
 * probe
 * ============================================================================ */

bool edsk_probe(const uint8_t *data, size_t size, size_t file_size,
                int *confidence)
{
    (void)file_size;
    if (size < 34) return false;

    if (memcmp(data, "EXTENDED CPC DSK", 16) == 0) {
        *confidence = 95;
        return true;
    }
    if (memcmp(data, "MV - CPC", 8) == 0) {
        *confidence = 90;
        return true;
    }
    return false;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t edsk_open(uft_disk_t *disk, const char *path,
                              bool read_only)
{
    (void)read_only;

    edsk_plugin_data_t *pd = calloc(1, sizeof(edsk_plugin_data_t));
    if (!pd) return UFT_ERROR_NO_MEMORY;

    pd->ctx = edsk_parser_open(path);
    if (!pd->ctx) {
        free(pd);
        return UFT_ERROR_FORMAT_INVALID;
    }

    int tracks = 0, sides = 0;
    bool extended = false;
    char creator[16] = {0};
    edsk_parser_get_info(pd->ctx, &tracks, &sides, &extended,
                         creator, sizeof(creator));

    disk->plugin_data = pd;
    disk->geometry.cylinders = (uint16_t)tracks;
    disk->geometry.heads = (uint8_t)sides;
    disk->geometry.sectors = 9;         /* typical — corrected per track */
    disk->geometry.sector_size = 512;   /* typical */
    disk->geometry.total_sectors = (uint32_t)tracks * sides * 9;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void edsk_close(uft_disk_t *disk)
{
    edsk_plugin_data_t *pd = disk->plugin_data;
    if (pd) {
        edsk_parser_close(&pd->ctx);
        free(pd);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track — delegates to edsk_parser_read_track
 * ============================================================================ */

static uft_error_t edsk_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    edsk_plugin_data_t *pd = disk->plugin_data;
    if (!pd || !pd->ctx) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    edsk_track_info_t *et = NULL;
    if (edsk_parser_read_track(pd->ctx, cyl, head, &et) != 0 || !et) {
        return UFT_OK;  /* empty track, not an error */
    }

    /* Convert EDSK sectors to UFT sectors */
    for (int s = 0; s < et->sector_count; s++) {
        edsk_sector_info_t *es = &et->sectors[s];
        if (!es->data || es->data_size == 0) continue;

        uint16_t sz = es->data_size;
        if (sz > 8192) sz = 8192;

        uft_format_add_sector(track,
                              es->sector_id > 0 ? es->sector_id - 1 : 0,
                              es->data, sz,
                              (uint8_t)cyl, (uint8_t)head);
    }

    edsk_parser_free_track(&et);
    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_edsk = {
    .name         = "EDSK",
    .description  = "Extended DSK (Amstrad CPC/ZX Spectrum)",
    .extensions   = "dsk;edsk",
    .version      = 0x00030302,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe        = edsk_probe,
    .open         = edsk_open,
    .close        = edsk_close,
    .read_track   = edsk_read_track,
    .verify_track = uft_generic_verify_track,
    .spec_status  = UFT_SPEC_OFFICIAL_FULL,  /* CPCWiki publishes the full EDSK v3 specification */
};

UFT_REGISTER_FORMAT_PLUGIN(edsk)
