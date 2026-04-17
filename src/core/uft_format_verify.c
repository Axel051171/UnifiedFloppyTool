/**
 * @file uft_format_verify.c
 * @brief Generische verify_track-Implementierung für Format-Plugins
 *
 * Wird von allen Plugins verwendet die keine formatspezifische
 * Verify-Logik benötigen. Liest den Track und vergleicht Sektor-für-Sektor
 * mit der Referenz.
 *
 * Für Formate mit Weak-Bits, Timing oder Fuzzy-Bits muss eine eigene
 * verify_track-Funktion geschrieben werden (z.B. uft_atx_verify_track,
 * uft_stx_verify_track).
 */
#include "uft/uft_format_common.h"
#include "uft/uft_format_plugin.h"
#include <string.h>

uft_error_t uft_generic_verify_track(uft_disk_t *disk, int cyl, int head,
                                      const uft_track_t *reference) {
    if (!disk || !reference) return UFT_ERROR_INVALID_STATE;

    /* Lookup plugin via registry (disk->plugin nicht im Struct) */
    const uft_format_plugin_t *plugin = uft_get_format_plugin(disk->format);
    if (!plugin || !plugin->read_track) return UFT_ERROR_NOT_SUPPORTED;

    uft_track_t actual;
    memset(&actual, 0, sizeof(actual));

    uft_error_t err = plugin->read_track(disk, cyl, head, &actual);
    if (err != UFT_OK) {
        uft_track_free(&actual);
        return err;
    }

    if (actual.sector_count != reference->sector_count) {
        uft_track_free(&actual);
        return UFT_ERROR_VERIFY_FAILED;
    }

    for (size_t s = 0; s < reference->sector_count; s++) {
        const uft_sector_t *a = &actual.sectors[s];
        const uft_sector_t *r = &reference->sectors[s];

        /* Datenlänge muss passen */
        size_t alen = a->data_len ? a->data_len : a->data_size;
        size_t rlen = r->data_len ? r->data_len : r->data_size;
        if (alen != rlen || !a->data || !r->data) {
            uft_track_free(&actual);
            return UFT_ERROR_VERIFY_FAILED;
        }

        /* Byte-genauer Vergleich */
        if (memcmp(a->data, r->data, rlen) != 0) {
            uft_track_free(&actual);
            return UFT_ERROR_VERIFY_FAILED;
        }
    }

    uft_track_free(&actual);
    return UFT_OK;
}
