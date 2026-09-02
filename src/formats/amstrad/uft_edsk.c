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

/* MF-796: EINE Definition fuer beide Uebersetzungseinheiten. Hier
 * standen die Parser-Typen VON HAND nachdeklariert, unter Namen, die
 * im Parser etwas anderes bedeuten — gemessen 40 gegen 32 Byte je
 * Sektor, Datenzeiger bei +16 gegen +8. Die Folge: jede Spur jeder
 * EDSK-Datei kam mit null Sektoren zurueck, still, mit UFT_OK.
 * Begruendung vollstaendig in uft_edsk_parser.h. */
#include "uft_edsk_parser.h"

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

    edsk_track_t *et = NULL;
    int r = edsk_parser_read_track(pd->ctx, cyl, head, &et);
    /* MF-796: „unformatiert" und „gescheitert" trennen. Vorher gab jeder
     * der beiden Faelle UFT_OK mit leerer Spur zurueck — der Aufrufer
     * konnte nicht sehen, ob die Spur wirklich leer ist oder ob das Lesen
     * nicht gelungen ist. Fuer ein forensisches Werkzeug ist das die
     * schwerere der beiden Aussagen, und sie war die stille. */
    if (r == EDSK_TRACK_LEER) return UFT_OK;      /* wirklich leer */
    if (r != EDSK_TRACK_OK || !et) return UFT_ERROR_IO;

    /* Convert EDSK sectors to UFT sectors */
    for (int s = 0; s < et->sector_count; s++) {
        edsk_sector_t *es = &et->sectors[s];
        if (!es->data || es->actual_size == 0) continue;

        uint16_t sz = es->actual_size;
        if (sz > 8192) sz = 8192;

        uft_format_add_sector(track,
                              es->id_sector > 0 ? es->id_sector - 1 : 0,
                              es->data, sz,
                              (uint8_t)cyl, (uint8_t)head);
    }

    edsk_parser_free_track(&et);
    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

static const uft_plugin_feature_t uft_format_plugin_edsk_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_edsk = {
    .name         = "EDSK",
    .description  = "Extended DSK (Amstrad CPC/ZX Spectrum)",
    .extensions   = "dsk;edsk",
    .version      = 0x00030302,
    .format       = UFT_FORMAT_EDSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe        = edsk_probe,
    .open         = edsk_open,
    .close        = edsk_close,
    .read_track   = edsk_read_track,
    .verify_track = uft_generic_verify_track,
    .spec_status  = UFT_SPEC_OFFICIAL_FULL,  /* CPCWiki publishes the full EDSK v3 specification */
    .features = uft_format_plugin_edsk_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_edsk_features) / sizeof(uft_format_plugin_edsk_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(edsk)
