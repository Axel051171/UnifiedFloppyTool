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

/* ============================================================================
 * Weak-bit-tolerant verify (ATX/STX/PRO)
 *
 * Diese Formate speichern Weak-Bit-Positionen die bei jedem Read unterschiedliche
 * Werte liefern können. Verify ignoriert Bytes mit weak_mask != 0.
 * Wenn der Sektor als weak markiert ist aber keine weak_mask hat, wird
 * der Sektor-Inhalt akzeptiert (kann nicht reproduzierbar verifiziert werden).
 * ============================================================================ */

uft_error_t uft_weak_bit_verify_track(uft_disk_t *disk, int cyl, int head,
                                       const uft_track_t *reference) {
    if (!disk || !reference) return UFT_ERROR_INVALID_STATE;
    const uft_format_plugin_t *plugin = uft_get_format_plugin(disk->format);
    if (!plugin || !plugin->read_track) return UFT_ERROR_NOT_SUPPORTED;

    uft_track_t actual;
    memset(&actual, 0, sizeof(actual));

    uft_error_t err = plugin->read_track(disk, cyl, head, &actual);
    if (err != UFT_OK) { uft_track_free(&actual); return err; }

    if (actual.sector_count != reference->sector_count) {
        uft_track_free(&actual);
        return UFT_ERROR_VERIFY_FAILED;
    }

    for (size_t s = 0; s < reference->sector_count; s++) {
        const uft_sector_t *a = &actual.sectors[s];
        const uft_sector_t *r = &reference->sectors[s];

        size_t alen = a->data_len ? a->data_len : a->data_size;
        size_t rlen = r->data_len ? r->data_len : r->data_size;
        if (alen != rlen || !a->data || !r->data) {
            uft_track_free(&actual);
            return UFT_ERROR_VERIFY_FAILED;
        }

        /* Wenn Weak-Sektor ohne Mask: überspringen (nicht reproduzierbar) */
        if (a->weak && !a->weak_mask) continue;

        /* Mit Weak-Mask: nur Non-Weak-Bytes vergleichen.
         *
         * weak_mask Semantik (per uft_sector_t.weak_mask doc-comment in
         * include/uft/uft_types.h:340 "Per-byte weak bit flags"):
         *   weak_mask[i] == 0  ⇒  Byte i ist solid, byte-vergleichen
         *   weak_mask[i] != 0  ⇒  Byte i ist weak, überspringen
         *
         * Producers folgen alle dem per-byte-Pattern:
         *   - src/formats/atx/uft_atx.c:297       memset(..., 0xFF, ...)
         *   - src/recovery/uft_multiread_pipeline.c:305  weak_mask[i] = 1
         *
         * Frühere per-bit-Lesart (`weak_mask[b/8] & (1 << (b%8))`) hatte
         * 7/8 der wirklich-weak-Bytes fälschlich byte-verglichen — bei
         * multiread-Output (per-byte 1) hätten reine zufällige Treffer
         * den Verify zum Scheitern gebracht. ATX (per-byte 0xFF) hat
         * by-accident funktioniert weil 0xFF alle Bits gesetzt hat.
         * Per docs/AI_COLLABORATION.md / structured-reviewer Finding #2. */
        if (a->weak_mask) {
            for (size_t b = 0; b < rlen; b++) {
                if (a->weak_mask[b] == 0) {
                    if (a->data[b] != r->data[b]) {
                        uft_track_free(&actual);
                        return UFT_ERROR_VERIFY_FAILED;
                    }
                }
            }
        } else {
            /* Kein Weak-Sektor: normaler Vergleich */
            if (memcmp(a->data, r->data, rlen) != 0) {
                uft_track_free(&actual);
                return UFT_ERROR_VERIFY_FAILED;
            }
        }
    }

    uft_track_free(&actual);
    return UFT_OK;
}

/* ============================================================================
 * Flux-level verify (WOZ/IPF/KFX/MFI/PRI/UDI)
 *
 * Für Flux/Bitstream-Formate vergleicht nur Track-Größe und Raw-Daten,
 * da Sektor-Extraktion nicht immer möglich ist. Wenn beide Tracks raw_data
 * haben, wird memcmp verwendet. Ansonsten wird Sektor-Vergleich versucht.
 * ============================================================================ */

uft_error_t uft_flux_verify_track(uft_disk_t *disk, int cyl, int head,
                                   const uft_track_t *reference) {
    if (!disk || !reference) return UFT_ERROR_INVALID_STATE;
    const uft_format_plugin_t *plugin = uft_get_format_plugin(disk->format);
    if (!plugin || !plugin->read_track) return UFT_ERROR_NOT_SUPPORTED;

    uft_track_t actual;
    memset(&actual, 0, sizeof(actual));

    uft_error_t err = plugin->read_track(disk, cyl, head, &actual);
    if (err != UFT_OK) { uft_track_free(&actual); return err; }

    /* Primär: raw_data vergleichen wenn vorhanden */
    if (actual.raw_data && reference->raw_data) {
        size_t aln = actual.raw_bits ? (actual.raw_bits + 7) / 8 : actual.raw_size;
        size_t rln = reference->raw_bits ? (reference->raw_bits + 7) / 8 : reference->raw_size;
        if (aln != rln || memcmp(actual.raw_data, reference->raw_data, rln) != 0) {
            uft_track_free(&actual);
            return UFT_ERROR_VERIFY_FAILED;
        }
        uft_track_free(&actual);
        return UFT_OK;
    }

    /* Fallback: Sektor-Vergleich mit Weak-Bit-Toleranz */
    if (actual.sector_count != reference->sector_count) {
        uft_track_free(&actual);
        return UFT_ERROR_VERIFY_FAILED;
    }
    for (size_t s = 0; s < reference->sector_count; s++) {
        const uft_sector_t *a = &actual.sectors[s];
        const uft_sector_t *r = &reference->sectors[s];
        size_t alen = a->data_len ? a->data_len : a->data_size;
        size_t rlen = r->data_len ? r->data_len : r->data_size;
        if (alen != rlen || !a->data || !r->data) {
            uft_track_free(&actual);
            return UFT_ERROR_VERIFY_FAILED;
        }
        if (a->weak) continue;  /* Flux: weak-sectors gelten immer als OK */
        if (memcmp(a->data, r->data, rlen) != 0) {
            uft_track_free(&actual);
            return UFT_ERROR_VERIFY_FAILED;
        }
    }
    uft_track_free(&actual);
    return UFT_OK;
}
